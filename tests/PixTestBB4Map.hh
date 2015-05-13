#ifndef PIXTESTBB4MAP_H
#define PIXTESTBB4MAP_H

#include "PixTest.hh"

#include <TH1.h>
#include <TSpectrum.h>

class DLLEXPORT PixTestBB4Map: public PixTest {
public:
  PixTestBB4Map(PixSetup *, std::string);
  PixTestBB4Map();
  virtual ~PixTestBB4Map();
  virtual bool setParameter(std::string parName, std::string sval); 
  void init(); 
  void setToolTips();

  void doTest(); 
  void setVthrCompCalDel();
  TF1* fitPeaks(TH1D *h, TSpectrum &s, int npeaks);

private:
  int          fParNtrig; 
  ClassDef(PixTestBB4Map, 1); 

};
#endif
