#ifndef PIXTESTFORWARDBIAS_H
#define PIXTESTFORWARDBIAS_H

#include "PixTest.hh"

class DLLEXPORT PixTestForwardBias: public PixTest {
public:
  PixTestForwardBias(PixSetup *, std::string);
  PixTestForwardBias();
  virtual ~PixTestForwardBias();
  virtual bool setParameter(std::string parName, std::string sval); 
  void init(); 
  void setToolTips();
  void bookHist(std::string); 

  void runCommand(std::string); 
  void forwardBiasTest();

  void doTest(); 

private:

  uint16_t fParNtrig; 
  int      fParVcal; 
  std::string fParPort;

  ClassDef(PixTestForwardBias, 1)

};
#endif
