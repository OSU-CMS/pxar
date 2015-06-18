 // -- author: Jamie Antonelli
// Bump Bond (BB) test using a forward bias across the sensor
// any working BBs will saturate under forward bias and not respond to pixel alive test
// any broken BBs will still fire under forward bias

#include <stdlib.h>     /* atof, atoi */
#include <algorithm>    // std::find
#include <iostream>
#include "PixUtil.hh"
#include "PixTestForwardBias.hh"
#include "log.h"

#include <TStopwatch.h>
#include <TH2.h>

#ifdef BUILD_HV
#include "hvsupply.h"
#endif

using namespace std;
using namespace pxar;

ClassImp(PixTestForwardBias)

// ----------------------------------------------------------------------
PixTestForwardBias::PixTestForwardBias(PixSetup *a, std::string name) : PixTest(a, name), fParNtrig(0), fParVcal(-1), fParPort("") {
  PixTest::init();
  init(); 
  LOG(logDEBUG) << "PixTestForwardBias ctor(PixSetup &a, string, TGTab *)";
}


//----------------------------------------------------------
PixTestForwardBias::PixTestForwardBias() : PixTest() {
  LOG(logDEBUG) << "PixTestForwardBias ctor()";
}

// ----------------------------------------------------------------------
bool PixTestForwardBias::setParameter(string parName, string sval) {
  bool found(false);
  std::transform(parName.begin(), parName.end(), parName.begin(), ::tolower);
  for (unsigned int i = 0; i < fParameters.size(); ++i) {
    if (fParameters[i].first == parName) {
      found = true; 
      if (!parName.compare("ntrig")) {
	fParNtrig = static_cast<uint16_t>(atoi(sval.c_str())); 
	setToolTips();
      }
      if (!parName.compare("vcal")) {
	fParVcal = atoi(sval.c_str()); 
	setToolTips();
      }
      if(!parName.compare("port")) {
        fParPort = sval;
      }
      break;
    }
  }
  return found; 
}



// ----------------------------------------------------------------------
void PixTestForwardBias::runCommand(std::string command) {
  std::transform(command.begin(), command.end(), command.begin(), ::tolower);
  LOG(logDEBUG) << "running command: " << command;

  if (!command.compare("forwardBiastest")) {
    forwardBiasTest(); 
    return;
  }

  LOG(logDEBUG) << "did not find command ->" << command << "<-";
}


// ----------------------------------------------------------------------
void PixTestForwardBias::init() {
  LOG(logDEBUG) << "PixTestForwardBias::init()";

  setToolTips();
  fDirectory = gFile->GetDirectory(fName.c_str()); 
  if (!fDirectory) {
    fDirectory = gFile->mkdir(fName.c_str()); 
  } 
  fDirectory->cd(); 

}

// ----------------------------------------------------------------------
void PixTestForwardBias::setToolTips() {
  fTestTip    = string("send Ntrig \"calibrates\" and count how many hits were measured\n")
    + string("the result is a hitmap, not an efficiency map\n")
    + string("NOTE: VCAL is given in high range!")
    ;
  fSummaryTip = string("all ROCs are displayed side-by-side. Note the orientation:\n")
    + string("the canvas bottom corresponds to the narrow module side with the cable")
    ;
}


// ----------------------------------------------------------------------
void PixTestForwardBias::bookHist(string name) {
  fDirectory->cd(); 
  LOG(logDEBUG) << "nothing done with " << name; 
}


//----------------------------------------------------------
PixTestForwardBias::~PixTestForwardBias() {
  LOG(logDEBUG) << "PixTestForwardBias dtor";
}


// ----------------------------------------------------------------------
void PixTestForwardBias::doTest() {

#ifndef BUILD_HV
  LOG(logERROR) << "Not built with HV supply support.";
  return;
#else

  TStopwatch t;

  fDirectory->cd();
  PixTest::update(); 
  bigBanner(Form("PixTestForwardBias::doTest()"));

  forwardBiasTest();
  TH1 *h1 = (*fDisplayedHist); 
  h1->Draw(getHistOption(h1).c_str());
  PixTest::update(); 

  int seconds = t.RealTime(); 
  LOG(logINFO) << "PixTestForwardBias::doTest() done, duration: " << seconds << " seconds";
#endif
}


// ----------------------------------------------------------------------
void PixTestForwardBias::forwardBiasTest() {
  cacheDacs();
  fDirectory->cd();
  PixTest::update(); 
  string ctrlregstring = getDacsString("ctrlreg"); 
  banner(Form("PixTestForwardBias::forwardBiasTest() ntrig = %d, vcal = %d (ctrlreg = %s)", 
	      static_cast<int>(fParNtrig), static_cast<int>(fParVcal), ctrlregstring.c_str()));

  fApi->setDAC("vcal", fParVcal);

  fApi->_dut->testAllPixels(true);
  fApi->_dut->maskAllPixels(false);

  // connect to the HV supply
  double serialTimeout = 5.0;
  pxar::HVSupply *hv = new pxar::HVSupply(fParPort.c_str(), serialTimeout);

  // save original values in order to reset them at the end
  double originalVoltage = hv->getVolts();
  double originalCompliance = hv->getMicroampsLimit();

  hv->setMicroampsLimit(101);  // set current limit to just above 0.1 mA  
  hv->setMicroamps(100);  // set current to 0.1 mA


  fNDaqErrors = 0; 
  vector<TH2D*> test2 = efficiencyMaps("PixelForwardBias", fParNtrig, FLAG_FORCE_MASKED); 
  vector<int> deadPixel(test2.size(), 0); 
  vector<int> probPixel(test2.size(), 0); 
  for (unsigned int i = 0; i < test2.size(); ++i) {
    fHistOptions.insert(make_pair(test2[i], "colz"));
    // -- count dead pixels
    for (int ix = 0; ix < test2[i]->GetNbinsX(); ++ix) {
      for (int iy = 0; iy < test2[i]->GetNbinsY(); ++iy) {
	if (test2[i]->GetBinContent(ix+1, iy+1) < fParNtrig) {
	  ++probPixel[i];
	  if (test2[i]->GetBinContent(ix+1, iy+1) < 1) {
	    ++deadPixel[i];
	  }
	}
      }
    }
  }

  copy(test2.begin(), test2.end(), back_inserter(fHistList));
  
  TH2D *h = (TH2D*)(fHistList.back());

  if (h) {
    h->Draw(getHistOption(h).c_str());
    fDisplayedHist = find(fHistList.begin(), fHistList.end(), h);
    PixTest::update(); 
  }
  restoreDacs();

  // reset HV settings
  hv->setMicroampsLimit(originalCompliance);  
  hv->setVolts(originalVoltage);

  // -- summary printout
  string deadPixelString, probPixelString; 
  for (unsigned int i = 0; i < probPixel.size(); ++i) {
    probPixelString += Form(" %4d", probPixel[i]); 
    deadPixelString += Form(" %4d", deadPixel[i]); 
  }
  LOG(logINFO) << "PixTestForwardBias::forwardBiasTest() done" 
	       << (fNDaqErrors>0? Form(" with %d decoding errors", static_cast<int>(fNDaqErrors)):"");
  LOG(logINFO) << "number of dead pixels (per ROC): " << deadPixelString;
  LOG(logDEBUG) << "number of red-efficiency pixels: " << probPixelString;

  //  PixTest::hvOff();
  //  PixTest::powerOff(); 
}


