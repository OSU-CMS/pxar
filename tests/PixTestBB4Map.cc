// -- author: Jamie Antonelli
// simplified Bump Bonding test
// based on the BBMap test written by Wolfram and Urs
// this test uses a Vcal scan instead of a VthrComp scan

#include <sstream>   // parsing
#include <algorithm>  // std::find

#include <TArrow.h>
#include <TSpectrum.h>
#include <TMarker.h>
#include "TStopwatch.h"

#include "PixTestBB4Map.hh"
#include "PixUtil.hh"
#include "log.h"
#include "constants.h"   // roctypes

using namespace std;
using namespace pxar;


ClassImp(PixTestBB4Map)

//------------------------------------------------------------------------------
PixTestBB4Map::PixTestBB4Map(PixSetup *a, std::string name): PixTest(a, name), 
  fParNtrig(-1){
  PixTest::init();
  init();
  LOG(logDEBUG) << "PixTestBB4Map ctor(PixSetup &a, string, TGTab *)";
}


//------------------------------------------------------------------------------
PixTestBB4Map::PixTestBB4Map(): PixTest() {
  LOG(logDEBUG) << "PixTestBB4Map ctor()";
}



//------------------------------------------------------------------------------
bool PixTestBB4Map::setParameter(string parName, string sval) {

  std::transform(parName.begin(), parName.end(), parName.begin(), ::tolower);
  for (uint32_t i = 0; i < fParameters.size(); ++i) {
    if (fParameters[i].first == parName) {
      sval.erase(remove(sval.begin(), sval.end(), ' '), sval.end());

      stringstream s(sval);
      if (!parName.compare( "ntrig")) { 
	s >> fParNtrig; 
	setToolTips();
	return true;
      }
    }
  }
  return false;
}

//------------------------------------------------------------------------------
void PixTestBB4Map::init() {
  LOG(logDEBUG) << "PixTestBB4Map::init()";
  
  fDirectory = gFile->GetDirectory("BB4");
  if (!fDirectory) {
    fDirectory = gFile->mkdir("BB4");
  }
  fDirectory->cd();
}

// ----------------------------------------------------------------------
void PixTestBB4Map::setToolTips() {
  fTestTip = string( "Bump Bonding Test = threshold map for CalS");
  fSummaryTip = string("module summary");
}


//------------------------------------------------------------------------------
PixTestBB4Map::~PixTestBB4Map() {
  LOG(logDEBUG) << "PixTestBB4Map dtor";
}

//------------------------------------------------------------------------------
void PixTestBB4Map::doTest() {

  double NSIGMA = 4;

  TStopwatch t;

  setVthrCompCalDel();

  cacheDacs();
  PixTest::update();
  bigBanner(Form("PixTestBB4Map::doTest() Ntrig = %d", fParNtrig));
 
  fDirectory->cd();

  fApi->_dut->testAllPixels(true);
  fApi->_dut->maskAllPixels(false);

  int flag(FLAG_CALS);
  fApi->setDAC("ctrlreg", 4);     // high range

  int result(1);

  fNDaqErrors = 0; 
  // scurveMaps function is located in pxar/tests/PixTest.cc
  // generate a TH1 s-curve wrt Vcal for each pixel
  vector<TH1*>  thrmapsCals = scurveMaps("Vcal", "calSMap", fParNtrig,50, 200, 10, result, 1, flag);
  
  // create vector of 2D plots to hold rescaled threshold maps
  vector<TH2D*>  rescaledThrmaps;

  // -- relabel negative thresholds as 255 and create distribution list
  // a negative threshold implies that the fit failed to converge
  vector<TH1D*> dlist; 
  TH1D *h(0);
  for (unsigned int i = 0; i < thrmapsCals.size(); ++i) {
    // initialize 2D rescaled threshold plots
    TH2D* rescaledPlot = bookTH2D(Form("rescaledThr_C%d", i), 
				  Form("(thr-mean)/sigma (C%d)", i), 
				  52, 0., 52., 80, 0., 80.); 
    rescaledThrmaps.push_back(rescaledPlot);
    for (int ix = 0; ix < thrmapsCals[i]->GetNbinsX(); ++ix) {
      for (int iy = 0; iy < thrmapsCals[i]->GetNbinsY(); ++iy) {
	if (thrmapsCals[i]->GetBinContent(ix+1, iy+1) < 0) thrmapsCals[i]->SetBinContent(ix+1, iy+1, 255.);
      }
    }
    // make a 1D histogram using the Vcal for each ROC
    // (one entry per pixel)
    h = distribution((TH2D*)thrmapsCals[i], 256, 0., 256.);

    dlist.push_back(h); 
    fHistList.push_back(h); 
  }

  restoreDacs();

  // -- summary printout
  string bbString(""), bbCuts(""); 
  int nBadBumps(0); 
  int nPeaks(0), cutDead(0); 
  TSpectrum s; 
  TF1* fit(0);

  // loop over the 1D distributions (one for each ROC)
  for (unsigned int i = 0; i < dlist.size(); ++i) {
    h = (TH1D*)dlist[i];
    // search for peaks in the distribution
    // sigma = 5, threshold = 1%
    // peaks below threshold*max_peak_height are discarded
    // "nobackground" means it doesn't try to subtract a background
    //   from the distribution
    nPeaks = s.Search(h, 5, "nobackground", 0.01); 
    LOG(logDEBUG) << "found " << nPeaks << " peaks in " << h->GetName();
    // use fitPeaks algorithm to get the fitted gaussian of good bumps
    //    cutDead = fitPeaks(h, s, nPeaks); 
    fit = fitPeaks(h, s, nPeaks); 
    double mean = 0;
    double sigma = 0;
    if (fit != 0){
      mean = fit->GetParameter(1); 
      sigma = fit->GetParameter(2); 
    }
    // place cut at NSIGMA*sigma above mean of gaussian
    // +1 places cut to the right of the arrow
    cutDead = h->GetXaxis()->FindBin(mean + NSIGMA*sigma) + 1; 

    // bad bumps are those above cutDead
    nBadBumps = static_cast<int>(h->Integral(cutDead+1, h->FindBin(255)));
    bbString += Form(" %4d", nBadBumps); 
    bbCuts   += Form(" %4d", cutDead); 

    // draw an arrow on the plot to denote cutDead
    TArrow *pa = new TArrow(cutDead, 0.5*h->GetMaximum(), cutDead, 0., 0.06, "|>"); 
    pa->SetArrowSize(0.1);
    pa->SetAngle(40);
    pa->SetLineWidth(2);
    h->GetListOfFunctions()->Add(pa); 

    // fill 2D plots with (thr-mean)/sigma (things above NSIGMA are called bad)
    for (int ix = 1; ix <= thrmapsCals[i]->GetNbinsX(); ++ix) {
      for (int iy = 1; iy <= thrmapsCals[i]->GetNbinsY(); ++iy) {
	double content = thrmapsCals[i]->GetBinContent(ix, iy);
	double rescaledThr = (content-mean)/sigma;
	rescaledThrmaps[i]->Fill(ix-1,iy-1,rescaledThr);
	rescaledThrmaps[i]->SetMinimum(-5);
	rescaledThrmaps[i]->SetMaximum(5);
      }
    }
    fHistList.push_back(rescaledThrmaps[i]);
    fHistOptions.insert(make_pair(rescaledThrmaps[i], "colz")); 
  }



  if (h) {
    h->Draw();
    fDisplayedHist = find(fHistList.begin(), fHistList.end(), h);
  }
  PixTest::update(); 
  
  int seconds = t.RealTime();
  LOG(logINFO) << "PixTestBB4Map::doTest() done"
	       << (fNDaqErrors>0? Form(" with %d decoding errors: ", static_cast<int>(fNDaqErrors)):"") 
	       << ", duration: " << seconds << " seconds";
  LOG(logINFO) << "number of dead bumps (per ROC): " << bbString;
  LOG(logINFO) << "separation cut       (per ROC): " << bbCuts;

}



// ----------------------------------------------------------------------
TF1* PixTestBB4Map::fitPeaks(TH1D *h, TSpectrum &s, int npeaks) {

#if defined ROOT_MAJOR_VER && ROOT_MAJOR_VER > 5
  Double_t *xpeaks = s.GetPositionX();
#else
  Float_t *xpeaks = s.GetPositionX();
#endif

  // this function has been simplified wrt the BB test
  // we now only use the highest peak to fit
  // bumps more than NSIGMA sigma above the peak are called bad
  double bestHeight = 0;
  TF1* f(0);
  TF1* bestFit(0);

  // loop over the found peaks, fit each with gaussian
  // save position, width of the chosen peak
  for (int p = 0; p < npeaks; ++p) {
    double xp = xpeaks[p];
    // don't consider failed s-curve fits (peak at 255)
    if (xp > 250) {
      continue;
    }
    // don't consider dead pixels (peak at 0)
    if (xp < 10) {
      continue;
    }
    string name = Form("gauss_%d", p); 
    // fit a gaussian to the peak
    // (using pixels within +-50 DAC of peak)
    f = new TF1(name.c_str(), "gaus(0)", xp-25., xp+25.);
    int bin = h->GetXaxis()->FindBin(xp);
    double yp = h->GetBinContent(bin);
    // use yp at peak position as guess for height of fit
    // use xp as guess of mean of fit
    // use 2 as guess for width of fit
    f->SetParameters(yp, xp, 2.);
    h->Fit(f, "Q+"); 
    double height = h->GetFunction(name.c_str())->GetParameter(0); 
    // save the parameters of the highest peak
    if (height > bestHeight) {
      bestHeight = height;
      bestFit = f;
    }
  }

  return bestFit;
}


// ----------------------------------------------------------------------
void PixTestBB4Map::setVthrCompCalDel() {
  uint16_t FLAGS = FLAG_CALS;

  cacheDacs();
  fDirectory->cd();
  PixTest::update(); 
  banner(Form("PixTestBB4Map::setVthrCompCalDel()")); 

  string name("bb4VthrCompCalDel");

  fApi->setDAC("CtrlReg", 4); 
  fApi->setDAC("Vcal", 250); 

  fApi->_dut->testAllPixels(false);
  fApi->_dut->maskAllPixels(true);

  vector<uint8_t> rocIds = fApi->_dut->getEnabledRocIDs(); 

  TH1D *h1(0); 
  h1 = bookTH1D(Form("bb4CalDel"), Form("bb4CalDel"), rocIds.size(), 0., rocIds.size()); 
  h1->SetMinimum(0.); 
  h1->SetDirectory(fDirectory); 
  setTitles(h1, "ROC", "CalDel DAC"); 

  TH2D *h2(0); 

  vector<int> calDel(rocIds.size(), -1); 
  vector<int> vthrComp(rocIds.size(), -1); 
  vector<int> calDelE(rocIds.size(), -1);
  
  int ip = 0; 

  int row = 11;
  int col = 20;

  fApi->_dut->testPixel(row, col, true);
  fApi->_dut->maskPixel(row, col, false);

  
  map<int, TH2D*> maps; 
  for (unsigned int iroc = 0; iroc < rocIds.size(); ++iroc) {
    h2 = bookTH2D(Form("%s_c%d_r%d_C%d", name.c_str(), row, col, rocIds[iroc]), 
		  Form("%s_c%d_r%d_C%d", name.c_str(), row, col, rocIds[iroc]), 
		  255, 0., 255., 255, 0., 255.); 
    fHistOptions.insert(make_pair(h2, "colz")); 
    maps.insert(make_pair(rocIds[iroc], h2)); 
    h2->SetMinimum(0.); 
    h2->SetMaximum(fParNtrig); 
    h2->SetDirectory(fDirectory); 
    setTitles(h2, "CalDel", "VthrComp"); 
  }


  bool done = false;
  vector<pair<uint8_t, pair<uint8_t, vector<pixel> > > >  rresults;
  while (!done) {
    rresults.clear(); 
    int cnt(0); 
    try{
      rresults = fApi->getEfficiencyVsDACDAC("caldel", 0, 255, "vthrcomp", 0, 180, FLAGS, fParNtrig);
      done = true;
    } catch(DataMissingEvent &e){
      LOG(logCRITICAL) << "problem with readout: "<< e.what() << " missing " << e.numberMissing << " events"; 
      ++cnt;
      if (e.numberMissing > 10) done = true; 
    } catch(pxarException &e) {
      LOG(logCRITICAL) << "pXar execption: "<< e.what(); 
      ++cnt;
    }
    done = (cnt>5) || done;
  }
  
  fApi->_dut->testPixel(row, col, false);
  fApi->_dut->maskPixel(row, col, true);
    
  for (unsigned i = 0; i < rresults.size(); ++i) {
    pair<uint8_t, pair<uint8_t, vector<pixel> > > v = rresults[i];
    int idac1 = v.first; 
    pair<uint8_t, vector<pixel> > w = v.second;      
    int idac2 = w.first;
    vector<pixel> wpix = w.second;
    for (unsigned ipix = 0; ipix < wpix.size(); ++ipix) {
      maps[wpix[ipix].roc()]->Fill(idac1, idac2, wpix[ipix].value()); 
    }
  }
  for (unsigned int iroc = 0; iroc < rocIds.size(); ++iroc) {  
    h2 = maps[rocIds[iroc]];
    TH1D *hy = h2->ProjectionY("_py", 5, h2->GetNbinsX()); 
    double vcthrMax = hy->GetMaximum();
    double bottom   = hy->FindFirstBinAbove(0.5*vcthrMax); 
    double top      = hy->FindLastBinAbove(0.5*vcthrMax); 
    delete hy;

    // set VthrComp to fParDeltaVthrComp DAC below top edge of tornado 
    // i.e. fParDeltaVthrComp above noise level 
    double fParDeltaVthrComp = -25;
    if (fParDeltaVthrComp>0) {
      vthrComp[iroc] = bottom + fParDeltaVthrComp;
    } else {
      vthrComp[iroc] = top + fParDeltaVthrComp;
    }

    double fParFracCalDel = 0.5;

    TH1D *h0 = h2->ProjectionX("_px", vthrComp[iroc], vthrComp[iroc]); 
    double cdMax   = h0->GetMaximum();
    double cdFirst = h0->GetBinLowEdge(h0->FindFirstBinAbove(0.5*cdMax)); 
    double cdLast  = h0->GetBinLowEdge(h0->FindLastBinAbove(0.5*cdMax)); 
    calDelE[iroc] = static_cast<int>(cdLast - cdFirst);
    calDel[iroc] = static_cast<int>(cdFirst + fParFracCalDel*calDelE[iroc]);
    TMarker *pm = new TMarker(calDel[iroc], vthrComp[iroc], 21); 
    pm->SetMarkerColor(kWhite); 
    pm->SetMarkerSize(2); 
    h2->GetListOfFunctions()->Add(pm); 
    pm = new TMarker(calDel[iroc], vthrComp[iroc], 7); 
    pm->SetMarkerColor(kBlack); 
    pm->SetMarkerSize(0.2); 
    h2->GetListOfFunctions()->Add(pm); 
    delete h0;

    h1->SetBinContent(rocIds[iroc]+1, calDel[iroc]); 
    h1->SetBinError(rocIds[iroc]+1, 0.5*calDelE[iroc]); 
    LOG(logDEBUG) << "CalDel: " << calDel[iroc] << " +/- " << 0.5*calDelE[iroc];
    
    h2->Draw(getHistOption(h2).c_str());
    PixTest::update(); 
    //    pxar::mDelay(500); 
    
    fHistList.push_back(h2); 
  }

  fHistList.push_back(h1); 

  fDisplayedHist = find(fHistList.begin(), fHistList.end(), h2);
  PixTest::update(); 

  restoreDacs();
  string caldelString(""), vthrcompString(""); 
  for (unsigned int iroc = 0; iroc < rocIds.size(); ++iroc){
    if (calDel[iroc] > 0) {
      fApi->setDAC("CalDel", calDel[iroc], rocIds[iroc]);
      caldelString += Form("  %4d", calDel[iroc]); 
    } else {
      caldelString += Form(" _%4d", fApi->_dut->getDAC(rocIds[iroc], "caldel")); 
    }
    fApi->setDAC("VthrComp", vthrComp[iroc], rocIds[iroc]);
    vthrcompString += Form("  %4d", vthrComp[iroc]); 
  }

  // -- summary printout
  LOG(logINFO) << "PixTestBB4Map::setVthrCompCalDel() done";
  LOG(logINFO) << "CalDel:   " << caldelString;
  LOG(logINFO) << "VthrComp: " << vthrcompString;

}
