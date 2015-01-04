/*
  This is the energy component of the Amanzi code. 

  Copyright 2010-201x held jointly by LANS/LANL, LBNL, and PNNL. 
  Amanzi is released under the three-clause BSD License. 
  The terms of use and "as is" disclaimer for this license are 
  provided in the top-level COPYRIGHT file.

  Authors: Ethan Coon
           Konstantin Lipnikov (lipnikov@lanl.gov)

  Process kernel for thermal Richards' flow.
*/

#ifndef AMANZI_ENERGY_TWO_PHASE_PK_HH_
#define AMANZI_ENERGY_TWO_PHASE_PK_HH_

#include "eos.hh"
#include "iem.hh"
#include "PK_Factory.hh"
#include "Energy_PK.hh"

namespace Amanzi {
namespace Energy {

class EnergyTwoPhase_PK : public Energy_PK {

public:
  EnergyTwoPhase_PK(Teuchos::RCP<const Teuchos::ParameterList>& glist, Teuchos::RCP<State>& S);
  virtual ~EnergyTwoPhase_PK() {};

  // Initialize owned (dependent) variables.
  virtual void Setup();
  virtual void Initialize();

 protected:
  // models for evaluating enthalpy
  Teuchos::RCP<Relations::EOS> eos_liquid_;
  Teuchos::RCP<IEM> iem_liquid_;

private:
  // factory registration
  // static RegisteredPKFactory<TwoPhase> reg_;
};

} // namespace Energy
} // namespace Amanzi

#endif
