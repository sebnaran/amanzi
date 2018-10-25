/* -*-  mode: c++; c-default-style: "google"; indent-tabs-mode: nil -*- */
// -------------------------------------------------------------
/**
 * @file   verify_deform.cc
 * @author Rao V. Garimella
 * @date   Thu Apr 12, 2012
 * 
 * @brief  
 * 
 * 
 */
// -------------------------------------------------------------
// -------------------------------------------------------------


#include <mpi.h>
#include <iostream>
#include <string>

#include "Teuchos_DefaultMpiComm.hpp"
#include "Teuchos_CommandLineProcessor.hpp"

#include "Mesh.hh"
#include "MeshFactory.hh"
#include "FrameworkTraits.hh"
#include "Geometry.hh"
#include "GMVMesh.hh"
// #include "Vis.hh"

int main(int argc, char *argv[]) {
  int ierr(0), aerr(0);

  MPI_Init(&argc, &argv);

  auto comm = Comm_ptr_type(new Teuchos::MpiComm<int>(MPI_COMM_WORLD));
  const int nproc(comm->getSize());
  const int me(comm->getRank());


  std::cerr << "Testing deformation code " << std::endl;


 

  Teuchos::CommandLineProcessor CLP;
  CLP.setDocString("Reads a serial exodus mesh file and a deformation file and writes out a deformed mesh");


  if (nproc > 1) {
    std::cerr << "Parallel deformation not implemented" << std::endl;
    assert (nproc == 1);
  }


  const Amanzi::AmanziMesh::Framework frameworks[] = {  
    Amanzi::AmanziMesh::STKMESH, 
    Amanzi::AmanziMesh::MSTK, 
  };
  const char *framework_names[] = {
    "stk::mesh", "MSTK"
  };

  const int numframeworks = sizeof(frameworks)/sizeof(Amanzi::AmanziMesh::Framework);

  Amanzi::AmanziMesh::Framework the_framework(Amanzi::AmanziMesh::MSTK);




  // Setup the argument lists with the documentation and whether they are 
  // necessary or not

  std::string mesh_filename;
  CLP.setOption("meshfile", &mesh_filename, "Name of mesh file", true);

  std::string def_filename;
  CLP.setOption("deffile", &def_filename, "Name of deformation file", true);

  
  CLP.setOption("framework", &the_framework, numframeworks, frameworks, 
                framework_names, "mesh framework preference", false);
  



  // Parse the command line

  Teuchos::CommandLineProcessor::EParseCommandLineReturn parseReturn;
  try {
    parseReturn = CLP.parse(argc, argv);
  } catch (const std::exception &e) {
    std::cerr << "error: " << e.what() << std::endl;
    ierr++;
  }
  comm.SumAll(&ierr, &aerr, 1);
  if (aerr > 0) return 1;
  
  if (parseReturn == Teuchos::CommandLineProcessor::PARSE_HELP_PRINTED)
    return 0;
  if (parseReturn != Teuchos::CommandLineProcessor::PARSE_SUCCESSFUL)
    return 1;



  // Create the mesh

  if (!Amanzi::AmanziMesh::framework_available(the_framework)) {
    std::cerr << "Chosen framework not available" << std::endl;
    exit(-1);
  }

  Amanzi::AmanziMesh::MeshFactory factory(&comm);
  Teuchos::RCP<Amanzi::AmanziMesh::Mesh> mesh;

  try {
    Amanzi::AmanziMesh::FrameworkPreference prefs(factory.preference());
    prefs.clear(); 
    prefs.push_back(the_framework);

    factory.preference(prefs);

    mesh = factory(mesh_filename);

  } catch (const Amanzi::AmanziMesh::Message& e) {
    std::cerr << ": mesh error: " << e.what() << std::endl;
    ierr++;
  } catch (const std::exception& e) {
    std::cerr << ": error: " << e.what() << std::endl;
    ierr++;
  }

  comm.SumAll(&ierr, &aerr, 1);

  if (aerr > 0) return 1;


  Amanzi::AmanziMesh::Entity_ID_List nodeids;
  Amanzi::AmanziGeometry::Point_List newpos, finpos;

  // Read the deformation file

  std::ifstream deffile;

  deffile.open(def_filename.c_str());
  if (deffile.is_open()) {

    int spdim = mesh->space_dimension();

    int nnodes, nnodes_in;
    nnodes = mesh->num_entities(Amanzi::AmanziMesh::NODE,
                                Amanzi::AmanziMesh::Parallel_type::OWNED);

    Amanzi::AmanziMesh::Entity_ID nodeid;

    try {
      int spdim1;
      deffile >> spdim1 >> nnodes_in;

      assert(spdim == spdim1);

      double defarr[nnodes][3];

      for (int i = 0; i < nnodes; i++)
        defarr[i][0] = defarr[i][1] = defarr[i][2] = 0.0;

      if (spdim == 2) {

        for (int i = 0; i < nnodes_in; i++) {
          double def[2];
          deffile >> nodeid >> def[0] >> def[1];          
          defarr[nodeid-1][0] = def[0];
          defarr[nodeid-1][1] = def[1];
          std::cerr << nodeid << " " << def[0] << " " << def[1] << std::endl;
        }

      }
      else if (spdim == 3) {

        for (int i = 0; i < nnodes_in; i++) {
          double def[3];
          deffile >> nodeid >> def[0] >> def[1] >> def[2];          
          defarr[nodeid-1][0] = def[0];
          defarr[nodeid-1][1] = def[1];
          defarr[nodeid-1][2] = def[2];
          std::cerr << nodeid << " " << def[0] << " " << def[1] << " " << def[2] << std::endl;
        }
        
      }

      for (int j = 0; j < nnodes; j++) {
        Amanzi::AmanziGeometry::Point oldcoord(spdim),newcoord(spdim);
        Amanzi::AmanziGeometry::Point defvec(spdim);
        
        mesh->node_get_coordinates(j,&oldcoord); 

        defvec.set(defarr[j]);
        newcoord = oldcoord + defvec;
        
        nodeids.push_back(j);    
        newpos.push_back(newcoord);
      }
  
    }
    catch (const std::exception &e) {
      std::cerr << "Error reading deformations" << std::endl;
      exit(-1);
    }
  }
  else {
    std::cerr << "Cannot open deformations file " << def_filename << std::endl;
    exit(-1);
  }

  deffile.close();


  // Deform the mesh
  
  int status;

  try {
    status = mesh->deform(nodeids,newpos,true,&finpos);

    if (status == 0)
      std::cerr << "Could not deform the mesh as much as requested" << std::endl;
  }
  catch (const std::exception &e) {
    std::cerr << "Error deforming the mesh" << std::endl;
    exit(-1);
  }


  
  std::string viz_filename("deform.gmv");
  Amanzi::GMV::create_mesh_file(*mesh,viz_filename);


  MPI_Finalize();
}



