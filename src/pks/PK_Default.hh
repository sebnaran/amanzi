/* -*-  mode: c++; indent-tabs-mode: nil -*- */
/*
  Amanzi is released under the three-clause BSD License. 
  The terms of use and "as is" disclaimer for this license are 
  provided in the top-level COPYRIGHT file.

  Author: Ethan Coon (ecoon@lanl.gov)
*/

//! The default, landing implementation for all PKs.

/*!  

PKs are formed through mixins, and therefore have a base implementation to
form the bottom of the class heirarchy.  Since Mixin classes don't typically
know where in the heirarchy they sit, they must assume that someone below them
in the heirarchy will do more work.  Therefore they always call the inherited
Base_t class's methods in addition to doing their own work.

This provides a landing for that call.  PK_Default should always be the
bottom-most class in the heirarchy.

For more, see PK.hh  

*/

#ifndef AMANZI_PK_DEFAULT_HH_
#define AMANZI_PK_DEFAULT_HH_

#include "Teuchos_RCP.hpp"
#include "Teuchos_ParameterList.hpp"

#include "StateDefs.hh"
#include "PK.hh"

namespace Amanzi {

class State;
class Debugger;

class PK_Default {
 public:

  // lone constructor
  PK_Default(const Teuchos::RCP<Teuchos::ParameterList>& pk_tree,
             const Teuchos::RCP<Teuchos::ParameterList>& global_plist,
             const Teuchos::RCP<State>& S);


  // Setup: forms the DAG, pushes meta-data into State
  // Default: no setup
  void Setup() {}
  
  // Initialize: initial conditions for owned variables.
  // Default: no initialization
  void Initialize() {}
  
  // Returns validity of the step taken from tag_old to tag_new
  // Default: step is valid
  bool ValidStep(const Key& tag_old, const Key& tag_new) { return true; }

  // Do work that can only be done if we know the step was successful.
  // Default: no work to be done
  void CommitStep(const Key& tag_old, const Key& tag_new) {}

  // Revert a step from tag_new back to tag_old
  // Default: no work to be done
  void FailStep(const Key& tag_old, const Key& tag_new) {}

  // Calculate any diagnostics at tag, currently used for visualization.
  // Default: no work to be done
  void CalculateDiagnostics(const Key& tag) {}

  // Return PK's name
  std::string name() { return name_; }

  // Accessor for debugger, for use by coupling MPCs
  Teuchos::Ptr<Debugger> debugger() { return Teuchos::null; }
  
 protected:
  // my subtree of the solution vector
  //  Teuchos::RCP<TreeVector> solution_;

  // state
  Teuchos::RCP<State> S_;

  // fancy IO
  Teuchos::RCP<VerboseObject> vo_;

  // my parameterlist
  Teuchos::RCP<Teuchos::ParameterList> plist_;
  
  // my name
  std::string name_;
  
};

}  // namespace Amanzi

#endif
