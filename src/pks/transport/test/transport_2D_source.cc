/*
  The transport component of the Amanzi code, serial unit tests.
  License: BSD
  Author: Konstantin Lipnikov (lipnikov@lanl.gov)
*/

#include <iostream>
#include <cstdlib>
#include <cmath>
#include <vector>

// TPLs
#include "Teuchos_ParameterList.hpp"
#include "Teuchos_RCP.hpp"
#include "Teuchos_ParameterXMLFileReader.hpp"
#include "Teuchos_XMLParameterListHelpers.hpp"
#include "UnitTest++.h"

// Amanzi
#include "GMVMesh.hh"
#include "MeshAudit.hh"
#include "MeshFactory.hh"
#include "State.hh"

// Transport
#include "TransportExplicit_PK.hh"

/* **************************************************************** */
TEST(TRANSPORT_SOURCE_2D_MESH) {
  using namespace Teuchos;
  using namespace Amanzi;
  using namespace Amanzi::AmanziMesh;
  using namespace Amanzi::Transport;
  using namespace Amanzi::AmanziGeometry;

std::cout << "Test: 2D transport on a square mesh for long time" << std::endl;
#ifdef HAVE_MPI
  Comm_ptr_type comm = Amanzi::getDefaultComm();
#else
  Epetra_SerialComm* comm = new Epetra_SerialComm();
#endif

  /* read parameter list */
  std::string xmlFileName = "test/transport_2D_source.xml";
  Teuchos::RCP<Teuchos::ParameterList> plist = Teuchos::getParametersFromXmlFile(xmlFileName);

  /* create a mesh framework */
  ParameterList region_list = plist->get<Teuchos::ParameterList>("regions");
  Teuchos::RCP<Amanzi::AmanziGeometry::GeometricModel> gm =
      Teuchos::rcp(new Amanzi::AmanziGeometry::GeometricModel(2, region_list, *comm));

  Preference pref;
  pref.clear();
  pref.push_back(Framework::MSTK);
  pref.push_back(Framework::STK);

  MeshFactory meshfactory(comm,gm);
  meshfactory.set_preference(pref);
  RCP<const Mesh> mesh = meshfactory.create("test/rect2D_50x50_ss.exo");
  
  /* create a simple state and populate it */
  Amanzi::VerboseObject::global_hide_line_prefix = true;

  std::vector<std::string> component_names;
  component_names.push_back("Component 0");
  component_names.push_back("Component 1");

  Teuchos::ParameterList state_list = plist->sublist("state");
  RCP<State> S = rcp(new State(state_list));
  S->RegisterDomainMesh(rcp_const_cast<Mesh>(mesh));

  TransportExplicit_PK TPK(plist, S, "transport", component_names);
  TPK.Setup(S.ptr());
  TPK.CreateDefaultState(mesh, 2);
  S->InitializeFields();
  S->InitializeEvaluators();

  /* modify the default state for the problem at hand */
  std::string passwd("state"); 
  Teuchos::RCP<Epetra_MultiVector> 
      flux = S->GetFieldData("darcy_flux", passwd)->ViewComponent("face", false);

  AmanziGeometry::Point velocity(1.0, 0.5);
  int nfaces_owned = mesh->num_entities(AmanziMesh::FACE, AmanziMesh::Parallel_type::OWNED);
  for (int f = 0; f < nfaces_owned; f++) {
    const AmanziGeometry::Point& normal = mesh->face_normal(f);
    (*flux)[0][f] = velocity * normal;
  }

  /* initialize a transport process kernel */
  TPK.Initialize(S.ptr());
 
  /* advance the transport state */
  int iter, k;
  double t_old(0.0), t_new(0.0), dt;

  Teuchos::RCP<Epetra_MultiVector> 
      tcc = S->GetFieldData("total_component_concentration", passwd)->ViewComponent("cell", false);

  iter = 0;
  bool flag = true;
  while (t_new < 0.3) {
    dt = TPK.StableTimeStep();
    t_new = t_old + dt;

    TPK.AdvanceStep(t_old, t_new);
    TPK.CommitStep(t_old, t_new, S);

    t_old = t_new;
    iter++;

    if (t_new > 0.1 && flag) {
      flag = false;
      if (TPK.MyPID == 0) {
        GMV::open_data_file(*mesh, (std::string)"transport.gmv");
        GMV::start_data();
        GMV::write_cell_data(*tcc, 0, "Component_0");
        GMV::close_data_file();
      }
      break;
    }
  }

  TPK.VV_CheckTracerBounds(*tcc, 0, 0.0, 1.0, Transport::TRANSPORT_LIMITER_TOLERANCE);
 
  
}





