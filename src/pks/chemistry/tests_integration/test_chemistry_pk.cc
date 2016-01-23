/* -*-  mode: c++; c-default-style: "google"; indent-tabs-mode: nil -*- */
#include <cstdlib>
#include <cmath>
#include <iostream>
#include <vector>
#include <typeinfo>

#include <UnitTest++.h>

#include "Teuchos_RCP.hpp"
#include "Teuchos_ParameterList.hpp"
#include "Teuchos_ParameterXMLFileReader.hpp"
#include "Teuchos_XMLParameterListHelpers.hpp"
#include "Epetra_SerialComm.h"
#include "XMLParameterListWriter.hh"

#include "MeshFactory.hh"
#include "GenerationSpec.hh"
#include "State.hh"

#include "Amanzi_PK.hh"
#include "Chemistry_State.hh"
#include "species.hh"
#include "chemistry_exception.hh"

#include "dbc.hh"
#include "errors.hh"
#include "exceptions.hh"

/*****************************************************************************
 **
 **  Tests for trilinos based chemistry process kernel in chemistry-pk.cc
 **
 *****************************************************************************/

SUITE(GeochemistryTestsChemistryPK) {
  namespace ac = Amanzi::AmanziChemistry;
  namespace ag = Amanzi::AmanziGeometry;
  namespace am = Amanzi::AmanziMesh;

  /*****************************************************************************
   **
   **  Common testing code
   **
   *****************************************************************************/
  class ChemistryPKTest {
   public:
    ChemistryPKTest();
    ~ChemistryPKTest();
 
    void RunTest(const std::string name, double* gamma);

   protected:
    ac::Amanzi_PK* cpk_;
    Teuchos::RCP<Teuchos::ParameterList> glist_;
    Teuchos::RCP<ac::Chemistry_State> chemistry_state_;
    Teuchos::RCP<Amanzi::State> state_;
    Teuchos::RCP<Amanzi::AmanziMesh::Mesh> mesh_;

   private:
    Epetra_SerialComm* comm_;
    ag::GeometricModelPtr gm_;
  };  // end class SpeciationTest

  ChemistryPKTest::ChemistryPKTest() {
    // assume that no errors or exceptions will occur in the
    // mesh/state related code....
    
    // get the parameter list from the input file.
    std::string xml_input_filename("test_chemistry_pk_native.xml");
    glist_ = Teuchos::getParametersFromXmlFile(xml_input_filename);

    // create a test mesh
    comm_ = new Epetra_SerialComm();
    Teuchos::ParameterList mesh_parameter_list =
      glist_->sublist("Mesh").sublist("Unstructured").sublist("Generate Mesh");

    am::GenerationSpec g(mesh_parameter_list);
    
    Teuchos::ParameterList region_parameter_list = glist_->sublist("Regions");
    gm_ = new ag::GeometricModel(3, region_parameter_list, (const Epetra_MpiComm *)comm_);
  
    am::FrameworkPreference pref;
    pref.clear();
    pref.push_back(am::Simple);

    am::MeshFactory meshfactory((Epetra_MpiComm *)comm_);
    meshfactory.preference(pref);

    mesh_ = meshfactory(mesh_parameter_list, gm_);

    // get the state parameter list and create the state object
    Teuchos::ParameterList state_parameter_list = glist_->sublist("state");

    state_ = Teuchos::rcp(new Amanzi::State(state_parameter_list));
    state_->RegisterDomainMesh(mesh_);

    // create the chemistry state object
    Teuchos::ParameterList chemistry_parameter_list = glist_->sublist("PKs").sublist("Chemistry");
    std::vector<std::string> component_names;
    component_names.push_back("Al+++");
    component_names.push_back("H+");
    component_names.push_back("HP04--");
    component_names.push_back("SiO2(aq)");
    component_names.push_back("UO2++");
    chemistry_state_ = Teuchos::rcp(new ac::Chemistry_State(chemistry_parameter_list, component_names, state_));
    chemistry_state_->Setup();
  }

  ChemistryPKTest::~ChemistryPKTest() {
    //delete cpk_;
    delete comm_;
    delete gm_;
  }

  void ChemistryPKTest::RunTest(const std::string name, double * gamma) {
  }  // end ChemistryPKTest::RunTest()

  /*****************************************************************************
   **
   **  individual tests
   **
   *****************************************************************************/
  TEST_FIXTURE(ChemistryPKTest, ChemistryPK_constructor) {
    // just make sure that we can have all the pieces together to set
    // up a chemistry process kernel....
    try {
      cpk_ = new ac::Amanzi_PK(glist_, chemistry_state_, state_, mesh_);
    } catch (ac::ChemistryException chem_error) {
      std::cout << chem_error.what() << std::endl;
    } catch (std::exception e) {
      std::cout << e.what() << std::endl;
    }
    // debug flag should be set after the constructor is finished....
    CHECK_EQUAL(false, cpk_->debug());
  }  // end TEST_FIXTURE()

  TEST_FIXTURE(ChemistryPKTest, ChemistryPK_initialize) {
    // make sure that we can initialize the pk and internal chemistry
    // object correctly based on the xml input....
    try {
      cpk_ = new ac::Amanzi_PK(glist_, chemistry_state_, state_, mesh_);
      cpk_->Setup();
      state_->Setup();
      state_->InitializeFields();
      chemistry_state_->Initialize();
      cpk_->InitializeChemistry();
    } catch (std::exception e) {
      std::cout << e.what() << std::endl;
      throw e;
    }
    // assume all is right with the world if we exited w/o an error
    CHECK_EQUAL(0, 0);
  }  // end TEST_FIXTURE()

  TEST_FIXTURE(ChemistryPKTest, ChemistryPK_get_chem_output_names) {
    try {
      cpk_ = new ac::Amanzi_PK(glist_, chemistry_state_, state_, mesh_);
      cpk_->Setup();
      state_->Setup();
      state_->InitializeFields();
      chemistry_state_->Initialize();
      cpk_->InitializeChemistry();
    } catch (std::exception e) {
      std::cout << e.what() << std::endl;
      throw e;
    }
    std::vector<std::string> names;
    cpk_->set_chemistry_output_names(&names);
    CHECK_EQUAL(names.at(0), "pH");
  }  // end TEST_FIXTURE()

}  // end SUITE(GeochemistryTestChemistryPK)
