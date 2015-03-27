/*
  License: see $AMANZI_DIR/COPYRIGHT
  Authors:: Daniil Svyatskiy

  Temporary wrapper converting the Chemistry_PK, which inherits from 
  BDFFnBase<CompositeVector>, to use TreeVectors.
*/

#include "chemistry_pk.hh"
#include "Chemistry_PK_Wrapper.hh"

namespace Amanzi {
namespace AmanziChemistry {

Chemistry_PK_Wrapper::Chemistry_PK_Wrapper(Teuchos::ParameterList& pk_tree,
					   const Teuchos::RCP<Teuchos::ParameterList>& glist,
					   const Teuchos::RCP<State>& S,
					   const Teuchos::RCP<TreeVector>& soln) :
    S_(S),
    soln_(soln),
    glist_(glist)
{
  std::string pk_name = pk_tree.name();

  const char* result = pk_name.data();
  while ((result = std::strstr(result, "->")) != NULL) {
    result += 2;
    pk_name = result;   
  }

  // grab the component names
  if (glist_->isSublist("Cycle Driver")) {
    if (glist_->sublist("Cycle Driver").isParameter("component names")) {
      comp_names_ = glist_->sublist("Cycle Driver").get<Teuchos::Array<std::string> >("component names").toVector();
    } else{
      Errors::Message msg("Chemistry_PK_Wrapper: Cycle Driver has no input parameter component names.");
      Exceptions::amanzi_throw(msg);
    }
  }

  Teuchos::ParameterList chemistry_plist = glist_->sublist("PKs").sublist("Chemistry");
  chemistry_model_ = chemistry_plist.get<std::string>("chemistry model", "Amanzi");
  if (chemistry_model_ == "Alquimia") {
#ifndef ALQUIMIA_ENABLED
    Errors::Message msg;
    msg << "Alquimia chemistry model is not enabled for this build.\n";
    Exceptions::amanzi_throw(msg);
#endif
  } else if (chemistry_model_ != "Amanzi") {
    Errors::Message msg;
    msg << "Unknown chemistry model: " << chemistry_model_ << ".\n";
    Exceptions::amanzi_throw(msg);
  }

#ifdef ALQUIMIA_ENABLED
  if (chemistry_model_ == "Alquimia") {
    // Start up the chemistry engine.
    if (!chemistry_plist.isParameter("Engine")) {
      Errors::Message msg;
      msg << "No 'Engine' parameter found in the parameter list for 'Chemistry'.\n";
      Exceptions::amanzi_throw(msg);
    }
    if (!chemistry_plist.isParameter("Engine Input File")) {
      Errors::Message msg;
      msg << "No 'Engine Input File' parameter found in the parameter list for 'Chemistry'.\n";
      Exceptions::amanzi_throw(msg);
    }
    std::string chemEngineName = chemistry_plist.get<std::string>("Engine");
    std::string chemEngineInputFile = chemistry_plist.get<std::string>("Engine Input File");
    chem_engine_ = Teuchos::rcp(new AmanziChemistry::ChemistryEngine(chemEngineName, chemEngineInputFile));

    // Overwrite the component names with the primary species names from the engine.
    chem_engine_->GetPrimarySpeciesNames(comp_names_);
  }
#endif

  CS = Teuchos::rcp(new AmanziChemistry::Chemistry_State(chemistry_plist, comp_names_, S));

  // construct
  if (chemistry_model_ == "Alquimia") {
#ifdef ALQUIMIA_ENABLED
     pk_ = Teuchos::rcp(new Alquimia_Chemistry_PK(*glist, CS, chem_engine_));
#endif
  } else if (chemistry_model_ == "Amanzi") {
    pk_ = Teuchos::rcp(new Chemistry_PK(chemistry_plist, CS));
  }
}

}  // namespace AmanziChemistry
}  // namespace Amanzi