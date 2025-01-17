/*
  Chemistry 

  Copyright 2010-201x held jointly by LANS/LANL, LBNL, and PNNL. 
  Amanzi is released under the three-clause BSD License. 
  The terms of use and "as is" disclaimer for this license are 
  provided in the top-level COPYRIGHT file.

  Base class for sorption isotherms
*/

#ifndef AMANZI_CHEMISTRY_SORPTION_ISOTHERM_HH_
#define AMANZI_CHEMISTRY_SORPTION_ISOTHERM_HH_

#include <string>

#include<species.hh>

namespace Amanzi {
namespace AmanziChemistry {

class SorptionIsotherm {
 public:
  enum SorptionIsothermType { FREUNDLICH, LANGMUIR, LINEAR };

  SorptionIsotherm(const std::string name, const SorptionIsothermType type);
  virtual~SorptionIsotherm() {};

  virtual double Evaluate(const Species& primarySpecies) = 0;
  virtual double EvaluateDerivative(const Species& primarySpecies) = 0;

  virtual void Display(const Teuchos::RCP<VerboseObject>& vo) const {};

  virtual const std::vector<double>& GetParameters(void) = 0;
  virtual void SetParameters(const std::vector<double>& params) = 0;

  std::string name(void) const {
    return name_;
  }

  SorptionIsothermType isotherm_type(void) const {
    return isotherm_type_;
  }

 private:
  std::string name_;
  SorptionIsothermType isotherm_type_;
};

}  // namespace AmanziChemistry
}  // namespace Amanzi
#endif
