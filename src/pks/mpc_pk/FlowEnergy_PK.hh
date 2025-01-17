/*
  This is the mpc_pk component of the Amanzi code. 

  Copyright 2010-201x held jointly by LANS/LANL, LBNL, and PNNL. 
  Amanzi is released under the three-clause BSD License. 
  The terms of use and "as is" disclaimer for this license are 
  provided in the top-level COPYRIGHT file.

  Authors: Daniil Svyatskiy
           Konstantin Lipnikov

  Process kernel that strongly couples Flow PK with Energy PK.
*/

#ifndef AMANZI_FLOW_ENERGY_PK_HH_
#define AMANZI_FLOW_ENERGY_PK_HH_

#include "Teuchos_RCP.hpp"

#include "independent_variable_field_evaluator_fromfunction.hh"
#include "secondary_variable_field_evaluator.hh"
#include "PK_BDF.hh"
#include "PK_MPCStrong.hh"
#include "PK_Factory.hh"

namespace Amanzi {

class FlowEnergy_PK : public PK_MPCStrong<PK_BDF> {
 public:
  FlowEnergy_PK(Teuchos::ParameterList& pk_tree,
                const Teuchos::RCP<Teuchos::ParameterList>& glist,
                const Teuchos::RCP<State>& S,
                const Teuchos::RCP<TreeVector>& soln);

  // PK methods
  virtual void Setup(const Teuchos::Ptr<State>& S);

  // -- dt is the minimum of the sub pks
  // virtual double get_dt();
  // virtual void set_dt(double dt);

  // -- advance each sub pk from t_old to t_new.
  virtual bool AdvanceStep(double t_old, double t_new, bool reinit = false);
  // virtual void CommitStep(double t_old, double t_new);

  std::string name() { return "thermal richards"; } 

  // virtual void CalculateDiagnostics() {};

 private:
  const Teuchos::RCP<Teuchos::ParameterList>& glist_;
  Teuchos::RCP<const AmanziMesh::Mesh> mesh_;

  Teuchos::RCP<IndependentVariableFieldEvaluatorFromFunction> particle_density_eval;
  Teuchos::RCP<IndependentVariableFieldEvaluatorFromFunction> porosity_eval;
  Teuchos::RCP<IndependentVariableFieldEvaluatorFromFunction> saturation_liquid_eval;

  Teuchos::RCP<VerboseObject> vo_;

  // factory registration
  static RegisteredPKFactory<FlowEnergy_PK> reg_;
};

}  // namespace Amanzi
#endif
