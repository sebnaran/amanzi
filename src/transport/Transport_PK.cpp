/*
This is the transport component of the Amanzi code. 
License: BSD
Author: Konstantin Lipnikov (lipnikov@lanl.gov)
*/

#include <vector>

#include "Epetra_Vector.h"
#include "Epetra_IntVector.h"
#include "Epetra_MultiVector.h"
#include "Epetra_Import.h"
#include "Teuchos_RCP.hpp"

#include "Mesh.hh"
#include "errors.hh"
#include "tabular-function.hh"

#include "tensor.hpp"
#include "mfd3d.hpp"
#include "Explicit_TI_RK.hpp"

#include "Transport_PK.hpp"
#include "Reconstruction.hpp"

namespace Amanzi {
namespace AmanziTransport {

/* ******************************************************************
* We set up only default values and call Init() routine to complete
* each variable initialization
****************************************************************** */
Transport_PK::Transport_PK(Teuchos::ParameterList &parameter_list_MPC,
			   Teuchos::RCP<Transport_State> TS_MPC)
{ 
  parameter_list = parameter_list_MPC;
  number_components = TS_MPC->get_total_component_concentration()->NumVectors();

  TS = Teuchos::rcp(new Transport_State(*TS_MPC));

  dT = dT_debug = T_internal = 0.0;
  status = TRANSPORT_NULL;
  verbosity_level = 0;
  internal_tests = 0;
  dispersivity_model = TRANSPORT_DISPERSIVITY_MODEL_NULL;
  tests_tolerance = TRANSPORT_CONCENTRATION_OVERSHOOT;

  MyPID = 0;
  mesh_ = TS->get_mesh_maps();
  dim = mesh_->space_dimension();

  Init();  // must be moved out of the constructor (lipnikov@lanl.gov)
}


/* ******************************************************************
* Routine processes parameter list. It needs to be called only once
* on each processor.                                                     
****************************************************************** */
int Transport_PK::Init()
{
  TS_nextBIG = Teuchos::rcp(new Transport_State(*TS, CopyMemory) );  
  TS_nextMPC = Teuchos::rcp(new Transport_State(*TS_nextBIG, ViewMemory));

  const Epetra_Map& cmap = mesh_->cell_map(true);
  cmin = cmap.MinLID();
  cmax = cmap.MaxLID();

  number_owned_cells = mesh_->count_entities(AmanziMesh::CELL, AmanziMesh::OWNED);
  cmax_owned = cmin + number_owned_cells - 1;

  const Epetra_Map& fmap = mesh_->face_map(true);
  fmin = fmap.MinLID();
  fmax = fmap.MaxLID(); 

  number_owned_faces = mesh_->count_entities(AmanziMesh::FACE, AmanziMesh::OWNED);
  fmax_owned = fmin + number_owned_faces - 1;

  number_wghost_cells = cmax + 1;  // assume that enumartion starts with 0 
  number_wghost_faces = fmax + 1;

#ifdef HAVE_MPI
  const  Epetra_Comm & comm = cmap.Comm(); 
  MyPID = comm.MyPID();

  const Epetra_Map& source_cmap = mesh_->cell_map(false);
  const Epetra_Map& target_cmap = mesh_->cell_map(true);

  cell_importer = Teuchos::rcp(new Epetra_Import(target_cmap, source_cmap));

  const Epetra_Map& source_fmap = mesh_->face_map(false);
  const Epetra_Map& target_fmap = mesh_->face_map(true);

  face_importer = Teuchos::rcp(new Epetra_Import(target_fmap, source_fmap));
#endif
 
  process_parameter_list();

  upwind_cell_ = Teuchos::rcp(new Epetra_IntVector(fmap));  // The maps include both owned and ghosts
  downwind_cell_ = Teuchos::rcp(new Epetra_IntVector(fmap));

  // advection block installation
  current_component_ = -1;  // default value may be useful in the future
  component_ = Teuchos::rcp(new Epetra_Vector(cmap));
  component_next_ = Teuchos::rcp(new Epetra_Vector(cmap));

  advection_limiter = TRANSPORT_LIMITER_TENSORIAL;
  limiter_ = Teuchos::rcp(new Epetra_Vector(cmap));

  lifting.reset_field(mesh_, component_);
  lifting.Init();

  // dispersivity block installation
  if (dispersivity_model != TRANSPORT_DISPERSIVITY_MODEL_NULL) {  // populate arrays for dispersive transport
    harmonic_points.resize(number_wghost_faces);
    for (int f=fmin; f<=fmax; f++) harmonic_points[f].init(dim);

    harmonic_points_weight.resize(number_wghost_faces);
    harmonic_points_value.resize(number_wghost_faces);

    dispersion_tensor.resize(number_wghost_cells);
    for (int c=cmin; c<=cmax; c++) dispersion_tensor[c].init(dim, 2);
  }

  // boundary conditions installation at time T=0
  for (int i=0; i<bcs.size(); i++) bcs[i]->Compute(0.0);

  return 0;
}



/* ******************************************************************
* Routine processes parameter list. It needs to be called only once
* on each processor.                                                     
****************************************************************** */
void Transport_PK::process_parameter_list()
{
  Teuchos::RCP<AmanziMesh::Mesh> mesh = TS->get_mesh_maps();

  // global transport parameters
  cfl = parameter_list.get<double>("CFL", 1.0);

  spatial_disc_order = parameter_list.get<int>("spatial discretization order", 1);
  if (spatial_disc_order < 1 || spatial_disc_order > 2) spatial_disc_order = 1;
  temporal_disc_order = parameter_list.get<int>("temporal discretization order", 1);
  if (temporal_disc_order < 1 || temporal_disc_order > 2) temporal_disc_order = 1;

  string dispersivity_name = parameter_list.get<string>("dispersivity model", "none");
  if (dispersivity_name == "isotropic") { 
    dispersivity_model = TRANSPORT_DISPERSIVITY_MODEL_ISOTROPIC;
  }
  else if (dispersivity_name == "Bear") {
    dispersivity_model = TRANSPORT_DISPERSIVITY_MODEL_BEAR;
  }
  else if (dispersivity_name == "Lichtner") {
    dispersivity_model = TRANSPORT_DISPERSIVITY_MODEL_LICHTNER;
  }

  dispersivity_longitudinal = parameter_list.get<double>("dispersivity longitudinal", 0.0);
  dispersivity_transverse = parameter_list.get<double>("dispersivity transverse", 0.0);

  string advection_limiter_name = parameter_list.get<string>("advection limiter", "none");
  if (advection_limiter_name == "BarthJespersen") {
    advection_limiter = TRANSPORT_LIMITER_BARTH_JESPERSEN;
  }
  else if (advection_limiter_name == "Tensorial" || advection_limiter_name == "none") {
    advection_limiter = TRANSPORT_LIMITER_TENSORIAL;
  }
   
  // control parameter
  verbosity_level = parameter_list.get<int>("verbosity level", 0);
  internal_tests = parameter_list.get<string>("enable internal tests", "no") == "yes";
  tests_tolerance = parameter_list.get<double>("internal tests tolerance", TRANSPORT_CONCENTRATION_OVERSHOOT);
  dT_debug = parameter_list.get<double>("maximal time step", TRANSPORT_LARGE_TIME_STEP);
 
  // extract list of lists of boundary conditions
  Teuchos::ParameterList BCs_list;
  BCs_list = parameter_list.get<Teuchos::ParameterList>("Transport BCs");

  // populate the list of boundary influx functions
  int nBCs = BCs_list.get<int>("number of BCs");
  bcs.clear();
  bcs_tcc_index.clear();

  for (int n=0; n<nBCs; n++) {
    char bc_char_name[10];
    
    sprintf(bc_char_name, "BC %d", n);
    string bc_name(bc_char_name);

    if (!BCs_list.isSublist(bc_name)) {
      Errors::Message msg;
      msg << "Boundary condition with name " << bc_char_name << " does not exist" << "\n";
      Exceptions::amanzi_throw(msg);
    }
    Teuchos::ParameterList BC_list = BC_list.sublist(bc_name);  // A single sublist. 
 
    for (int i=0; i<number_components; i++) {
      char tcc_char_name[20];

      sprintf(tcc_char_name, "Component %d", i);
      string tcc_name(tcc_char_name);

      if (BC_list.isParameter(tcc_name)) { 
        std::vector<std::string> regions, functions;
        std::vector<double> times, values;

        regions = BC_list.get<Teuchos::Array<std::string> >("Regions").toVector();
        times = BC_list.get<Teuchos::Array<double> >("Times").toVector();
        values = BC_list.get<Teuchos::Array<double> >(tcc_name).toVector();
        functions = BC_list.get<Teuchos::Array<std::string> >("Time functions").toVector();

        int nfunctions = functions.size();  // convert strings to forms
        std::vector<TabularFunction::Form> forms;
        for (int k=0; k<nfunctions; k++) {
          forms[k] = (functions[k] == "Constant") ? TabularFunction::CONSTANT : TabularFunction::LINEAR;
        }

        Teuchos::RCP<Function> f;
        f = Teuchos::rcp(new TabularFunction(times, values, forms));

        BoundaryFunction* bnd_fun = new BoundaryFunction(mesh_);
        bnd_fun->Define(regions, f);
        bcs.push_back(bnd_fun);
        bcs_tcc_index.push_back(i);
        break;
      }
    }
  }
}




/* ************************************************************* */
/* Printing information about Transport status                   */
/* ************************************************************* */
void Transport_PK::print_statistics() const
{
  if (!MyPID && verbosity_level > 0) {
    cout << "Transport PK: CFL = " << cfl << endl;
    cout << "              Total number of components = " << number_components << endl;
    cout << "              Verbosity level = " << verbosity_level << endl;
    cout << "              spatial discretication order = " << spatial_disc_order << endl;
    cout << "              temporal discretication order = " << temporal_disc_order << endl;
    cout << "              Enable internal tests = " << (internal_tests ? "yes" : "no")  << endl;
  }
}




/* *******************************************************************
 * Estimation of the time step based on T.Barth (Lecture Notes   
 * presented at VKI Lecture Series 1994-05, Theorem 4.2.2.       
 * Routine must be called every time we update a flow field      
 ****************************************************************** */
double Transport_PK::calculate_transport_dT()
{
  // flow could not be available at initialization, copy it again 
  if (status == TRANSPORT_NULL) {
    TS->copymemory_multivector(TS->ref_total_component_concentration(), 
                               TS_nextBIG->ref_total_component_concentration());
    TS->copymemory_vector(TS->ref_darcy_flux(), TS_nextBIG->ref_darcy_flux());

    check_divergence_property();
    identify_upwind_cells();

    status = TRANSPORT_FLOW_AVAILABLE;
  }

  // loop over faces and accumulate upwinding fluxes
  int  i, f, c, c1;

  Teuchos::RCP<AmanziMesh::Mesh> mesh = TS->get_mesh_maps();
  const Epetra_Map& fmap = mesh->face_map(true);
  const Epetra_Vector& darcy_flux = TS_nextBIG->ref_darcy_flux();

  std::vector<double> total_influx(number_wghost_cells, 0.0);

  for (f=fmin; f<=fmax; f++) {
    c = (*downwind_cell_)[f];
    if (c >= 0) total_influx[c] += fabs(darcy_flux[f]); 
  }

  // loop over cells and calculate minimal dT
  double influx, dT_cell; 
  const Epetra_Vector& ws = TS->ref_water_saturation();
  const Epetra_Vector& phi = TS->ref_porosity();

  dT = dT_cell = TRANSPORT_LARGE_TIME_STEP;
  for (c=cmin; c<=cmax_owned; c++) {
    influx = total_influx[c];
    if (influx) dT_cell = mesh->cell_volume(c) * phi[c] * ws[c] / influx;

    dT = std::min(dT, dT_cell);
  }  
  if (spatial_disc_order == 2) dT /= 2;


#ifdef HAVE_MPI
  double dT_global;
  const  Epetra_Comm & comm = ws.Comm(); 
 
  comm.MinAll(&dT, &dT_global, 1);
  dT = dT_global;
#endif

  // incorporate developers and CFL constraints
  dT = std::min(dT, dT_debug);

  dT *= cfl;
  return dT;
}


/* ******************************************************************* 
 * MPC will call this function to advance the transport state    
 ****************************************************************** */
void Transport_PK::advance(double dT_MPC)
{
  T_internal += dT_MPC;
  if (spatial_disc_order == 1) {  // temporary solution (lipnikov@lanl.gov)
    advance_donor_upwind(dT_MPC);
  }
  else if (spatial_disc_order == 2 && temporal_disc_order == 1) {
    advance_second_order_upwind(dT_MPC);
  }
  else if (spatial_disc_order == 2 && temporal_disc_order == 2) {
    advance_second_order_upwind_testing(dT_MPC);
  }
}


/* ******************************************************************* 
 * We have to advance each component independently due to different
 * discretizations. We use tcc when only owned data are needed and 
 * tcc_next when owned and ghost data.
 *
 * Data flow: loop over components C and for each C apply the 
 * second-order RK method. 
 ****************************************************************** */
void Transport_PK::advance_second_order_upwind_testing(double dT_MPC)
{
  status = TRANSPORT_STATE_BEGIN;
  dT = dT_MPC;  // overwrite the transport step

  int i, f, v, c, c1, c2;
  const Epetra_Vector& darcy_flux = TS_nextBIG->ref_darcy_flux();
  const Epetra_Vector& ws  = TS_nextBIG->ref_water_saturation();
  const Epetra_Vector& phi = TS_nextBIG->ref_porosity();
 
  Teuchos::RCP<Epetra_MultiVector> tcc = TS->get_total_component_concentration();
  Teuchos::RCP<Epetra_MultiVector> tcc_next = TS_nextBIG->get_total_component_concentration();
 
  Explicit_TI::RK TVD_RK(*this,  // it has only one member function fun
                         Explicit_TI::RK::heun_euler,  // integration method
                         *component_);

  int num_components = tcc->NumVectors();
  for (i=0; i<num_components; i++) {
    current_component_ = i;  // it is needed in BJ called inside RK:fun

    Epetra_Vector*& tcc_component = (*tcc)(i);
    TS_nextBIG->copymemory_vector(*tcc_component, *component_);  // tcc is a short vector 

    const double t = 0.0;  // provide simulation time (lipnikov@lanl.gov)
    TVD_RK.step(t, dT, *component_, *component_next_);

    for (c=cmin; c<=cmax_owned; c++) (*tcc_next)[i][c] = (*component_next_)[c];

    /*
    // DISPERSION FLUXES
    if (dispersivity_model != TRANSPORT_DISPERSIVITY_MODEL_NULL) {
      calculate_dispersion_tensor();

      std::vector<int> bc_face_id(number_wghost_faces);  // must be allocated only once (lipnikov@lanl.gov)
      std::vector<double> bc_face_values(number_wghost_faces);

      extract_boundary_conditions(i, bc_face_id, bc_face_values);
      populate_harmonic_points_values(i, tcc, bc_face_id, bc_face_values);
      add_dispersive_fluxes(i, tcc, bc_face_id, bc_face_values, tcc_next);
    }
    */
  }

  if (internal_tests) {
    Teuchos::RCP<Epetra_MultiVector> tcc_nextMPC = TS_nextMPC->get_total_component_concentration();
    check_GEDproperty(*tcc_nextMPC);
  }

  status = TRANSPORT_STATE_COMPLETE;
}



/* ******************************************************************* 
 * We have to advance each component independently due to different
 * discretizations. We use tcc when only owned data are needed and 
 * tcc_next when owned and ghost data.
 ****************************************************************** */
void Transport_PK::advance_second_order_upwind(double dT_MPC)
{
  status = TRANSPORT_STATE_BEGIN;
  dT = dT_MPC;  // overwrite the transport step

  int i, f, v, c, c1, c2;

  const Epetra_Vector& darcy_flux = TS_nextBIG->ref_darcy_flux();
  const Epetra_Vector& ws  = TS_nextBIG->ref_water_saturation();
  const Epetra_Vector& phi = TS_nextBIG->ref_porosity();

  Teuchos::RCP<Epetra_MultiVector> tcc = TS->get_total_component_concentration();
  Teuchos::RCP<Epetra_MultiVector> tcc_next = TS_nextBIG->get_total_component_concentration();
  TS_nextBIG->copymemory_multivector(*tcc, *tcc_next);  // tcc_next=tcc for owned and ghost cells

  // prepare conservative state in master and slave cells 
  double u, vol_phi_ws, tcc_flux;
  int num_components = tcc->NumVectors();
 
  lifting.reset_field(mesh_, component_);
  lifting.Init();

  for (i=0; i<num_components; i++) {
    for (c=cmin; c<=cmax; c++) (*component_)[c] = (*tcc_next)[i][c];

    for (c=cmin; c<=cmax_owned; c++) {  // calculate conservative quantatity 
      vol_phi_ws = mesh_->cell_volume(c) * phi[c] * ws[c]; 
      (*tcc_next)[i][c] = (*tcc)[i][c] * vol_phi_ws;  // after this tcc_next=tcc only for ghost cells
    }

    lifting.reset_field(mesh_, component_);
    lifting.calculateCellGradient();

    Teuchos::RCP<Epetra_MultiVector> gradient = lifting.get_gradient();
    std::vector<double>& field_local_min = lifting.get_field_local_min();  // not used (lipnikov@lanl.gov)
    std::vector<double>& field_local_max = lifting.get_field_local_max();

    limiterBarthJespersen(
        i, component_, gradient, field_local_min, field_local_max, limiter_);
    lifting.applyLimiter(limiter_);
 
    // ADVECTIVE FLUXES
    for (f=fmin; f<=fmax; f++) {  // loop over master and slave faces
      c1 = (*upwind_cell_)[f]; 
      c2 = (*downwind_cell_)[f]; 

      u = fabs(darcy_flux[f]);
      const AmanziGeometry::Point& xf = mesh_->face_centroid(f);

      if (c1 >=0 && c1 <= cmax_owned && c2 >= 0 && c2 <= cmax_owned) {
        double upwind_tcc = lifting.getValue(c1, xf);
        tcc_flux = dT * u * upwind_tcc;
        (*tcc_next)[i][c1] -= tcc_flux;
        (*tcc_next)[i][c2] += tcc_flux;
      } 
      else if (c1 >=0 && c1 <= cmax_owned && (c2 > cmax_owned || c2 < 0)) {
        double upwind_tcc = lifting.getValue(c1, xf);
        tcc_flux = dT * u * upwind_tcc;
        (*tcc_next)[i][c1] -= tcc_flux;
      } 
      else if (c1 > cmax_owned && c2 >= 0 && c2 <= cmax_owned) {
        double upwind_tcc = lifting.getValue(c1, xf);
        tcc_flux = dT * u * upwind_tcc;
        (*tcc_next)[i][c2] += tcc_flux;
      }
    } 

    // BOUNDARY CONDITIONS for ADVECTION
    for (int n=0; n<bcs.size(); n++) {  // analyze boundary sets
      if (i == bcs_tcc_index[n]) {
        for (BoundaryFunction::Iterator bc=bcs[n]->begin(); bc != bcs[n]->end(); ++bc) {
          f = bc->first;
          c2 = (*downwind_cell_)[f]; 

          if (c2 >= 0) {
            u = fabs(darcy_flux[f]);
            tcc_flux = dT * u * bc->second;
            (*tcc_next)[i][c2] += tcc_flux;
          }
        } 
      }
    }
    
    // DISPERSIVE FLUXES
    if (dispersivity_model != TRANSPORT_DISPERSIVITY_MODEL_NULL) {
      calculate_dispersion_tensor();

      std::vector<int> bc_face_id(number_wghost_faces);  // must be allocated only once (lipnikov@lanl.gov)
      std::vector<double> bc_face_values(number_wghost_faces);

      extract_boundary_conditions(i, bc_face_id, bc_face_values);
      populate_harmonic_points_values(i, tcc, bc_face_id, bc_face_values);
      add_dispersive_fluxes(i, tcc, bc_face_id, bc_face_values, tcc_next);
    }
    
    for (c=cmin; c<=cmax_owned; c++) {  // recover concentration from new conservative state
      vol_phi_ws = mesh_->cell_volume(c) * phi[c] * ws[c]; 
      (*tcc_next)[i][c] = (*tcc_next)[i][c] / vol_phi_ws;
    }
  }

  if (internal_tests) {
    Teuchos::RCP<Epetra_MultiVector> tcc_nextMPC = TS_nextMPC->get_total_component_concentration();
    check_GEDproperty(*tcc_nextMPC);
  }

  status = TRANSPORT_STATE_COMPLETE;
}



/* ******************************************************************* 
 * A simple first-order transport method 
 ****************************************************************** */
void Transport_PK::advance_donor_upwind(double dT_MPC)
{
  status = TRANSPORT_STATE_BEGIN;
  dT = dT_MPC;  // overwrite the transport step

  const Epetra_Vector& darcy_flux = TS_nextBIG->ref_darcy_flux();
  const Epetra_Vector& ws  = TS_nextBIG->ref_water_saturation();
  const Epetra_Vector& phi = TS_nextBIG->ref_porosity();

  // populating next state of concentrations
  Teuchos::RCP<Epetra_MultiVector> tcc = TS->get_total_component_concentration();
  Teuchos::RCP<Epetra_MultiVector> tcc_next = TS_nextBIG->get_total_component_concentration();
  TS_nextBIG->copymemory_multivector(*tcc, *tcc_next);

  // prepare conservative state in master and slave cells 
  double vol_phi_ws, tcc_flux;
  int num_components = tcc->NumVectors();

  for (int c=cmin; c<=cmax_owned; c++) {
    vol_phi_ws = mesh_->cell_volume(c) * phi[c] * ws[c]; 

    for (int i=0; i<num_components; i++) 
      (*tcc_next)[i][c] = (*tcc)[i][c] * vol_phi_ws;
  }

  // advance all components at once
  double u;
  for (int f=fmin; f<=fmax; f++) {  // loop over master and slave faces
    int c1 = (*upwind_cell_)[f]; 
    int c2 = (*downwind_cell_)[f]; 

    u = fabs(darcy_flux[f]);

    if (c1 >=0 && c1 <= cmax_owned && c2 >= 0 && c2 <= cmax_owned) {
      for (int i=0; i<num_components; i++) {
        tcc_flux = dT * u * (*tcc)[i][c1];
        (*tcc_next)[i][c1] -= tcc_flux;
        (*tcc_next)[i][c2] += tcc_flux;
      }
    } 
    else if (c1 >=0 && c1 <= cmax_owned && (c2 > cmax_owned || c2 < 0)) {
      for (int i=0; i<num_components; i++) {
        tcc_flux = dT * u * (*tcc)[i][c1];
        (*tcc_next)[i][c1] -= tcc_flux;
      }
    } 
    else if (c1 > cmax_owned && c2 >= 0 && c2 <= cmax_owned) {
      for (int i=0; i<num_components; i++) {
        tcc_flux = dT * u * (*tcc_next)[i][c1];
        (*tcc_next)[i][c2] += tcc_flux;
      }
    } 
  }

  // loop over exterior boundary sets
  for (int n=0; n<bcs.size(); n++) {
    int i = bcs_tcc_index[n];
    for (BoundaryFunction::Iterator bc=bcs[n]->begin(); bc != bcs[n]->end(); ++bc) {
      int f = bc->first;
      int c2 = (*downwind_cell_)[f]; 

      if (c2 >= 0) {
        u = fabs(darcy_flux[f]);
        tcc_flux = dT * u * bc->second;
        (*tcc_next)[i][c2] += tcc_flux; 
      }
    }
  }

  // recover concentration from new conservative state
  for (int c=cmin; c<=cmax_owned; c++) {
    vol_phi_ws = mesh_->cell_volume(c) * phi[c] * ws[c]; 
    for (int i=0; i<num_components; i++) (*tcc_next)[i][c] /= vol_phi_ws;
  }

  if (internal_tests) {
    Teuchos::RCP<Epetra_MultiVector> tcc_nextMPC = TS_nextMPC->get_total_component_concentration();
    check_GEDproperty(*tcc_nextMPC);
  }

  status = TRANSPORT_STATE_COMPLETE;
}


/* *******************************************************************
 * Calculate a dispersive tensor the from Darcy fluxes. The flux is
 * assumed to be scaled by face area.
 ****************************************************************** */
void Transport_PK::calculate_dispersion_tensor() 
{
  const Epetra_Vector& darcy_flux = TS_nextBIG->ref_darcy_flux();
  const Epetra_Vector& ws  = TS_nextBIG->ref_water_saturation();
  const Epetra_Vector& phi = TS_nextBIG->ref_porosity();

  AmanziMesh::Entity_ID_List nodes, faces;
  AmanziGeometry::Point velocity(dim), flux(dim);
  WhetStone::Tensor T(dim, 2);

  for (int c=cmin; c<=cmax_owned; c++) {
    if (dispersivity_model == TRANSPORT_DISPERSIVITY_MODEL_ISOTROPIC) {
      for (int i=0; i<dim; i++) dispersion_tensor[c](i, i) = dispersivity_longitudinal;
    }
    else {
      mesh_->cell_get_nodes(c, &nodes);
      int nnodes = nodes.size();

      int num_good_corners = 0;
      for (int n=0; n<nnodes; n++) {
        int v = nodes[n];
        mesh_->node_get_cell_faces(v, c, AmanziMesh::USED, &faces);
        int nfaces = faces.size();
   
        for (int i=0; i<dim; i++) { 
          int f = faces[i];
          const AmanziGeometry::Point& normal = mesh_->face_normal(f);
          T.add_row(i, normal);
          flux[i] = darcy_flux[f];
        }

        T.inverse();
        velocity += T * flux;
        num_good_corners ++;  // each corners is good temporary (lipnikov@lanl.gov)
      }
      velocity /= num_good_corners;
  
      double velocity_value = norm(velocity);
      double anisotropy = dispersivity_longitudinal - dispersivity_transverse;

      for (int i=0; i<dim; i++) {
        dispersion_tensor[c](i, i) = dispersivity_transverse * velocity_value; 
        for (int j=i; j<dim; j++) {
          double s = anisotropy * velocity[i] * velocity[j];
          if (velocity_value) s /= velocity_value;
          dispersion_tensor[c](j, i) = dispersion_tensor[c](i, j) += s;           
        }
      }
    }
 
    double vol_phi_ws = mesh_->cell_volume(c) * phi[c] * ws[c]; 
    for (int i=0; i<dim; i++) dispersion_tensor[c](i, i) *= vol_phi_ws;
  }
}


/* *******************************************************************
 * Collect time-dependent boundary data in face-based arrays.                               
 ****************************************************************** */
void Transport_PK::extract_boundary_conditions(const int component,
                                               std::vector<int>& bc_face_id,
                                               std::vector<double>& bc_face_value) 
{
  bc_face_id.assign(number_wghost_faces, 0);

  for (int n=0; n<bcs.size(); n++) {
    if (component == bcs_tcc_index[n]) {
      for (BoundaryFunction::Iterator bc=bcs[n]->begin(); bc != bcs[n]->end(); ++bc) {
        int f = bc->first;
        bc_face_id[f] = TRANSPORT_BC_CONSTANT_TCC;
        bc_face_value[f] = bc->second;
      }
    }
  }
}


/* *******************************************************************
 * Calculate field values at harmonic points. For harmonic points on
 * domain boundary, we use Dirichlet boundary values.
 ****************************************************************** */
void Transport_PK::populate_harmonic_points_values(int component,
                                                   Teuchos::RCP<Epetra_MultiVector> tcc,
                                                   std::vector<int>& bc_face_id,
                                                   std::vector<double>& bc_face_values)
{
  WhetStone::MFD3D mfd(mesh_);
  AmanziMesh::Entity_ID_List cells;

  for (int f=fmin; f<fmax_owned; f++) {
    double weight;
    mfd.calculate_harmonic_points(f, dispersion_tensor, harmonic_points[f], weight);
    harmonic_points_weight[f] = weight;
    
    mesh_->face_get_cells(f, AmanziMesh::USED, &cells);
    int ncells = cells.size();

    if (ncells == 2) {
      harmonic_points_value[f] = weight * (*tcc)[component][cells[0]] 
                               + (1 - weight) * (*tcc)[component][cells[1]];
    }
    else if (bc_face_id[f] == TRANSPORT_BC_CONSTANT_TCC) {
      harmonic_points_value[f] = bc_face_values[f]; 
    }                                                   
    else {
      harmonic_points_value[f] = (*tcc)[component][cells[0]];  // ad-hoc solution (lipnikov@lanl.gov)
    }
  }
}


/* *******************************************************************
 * Calculate and add dispersive fluxes of the conservative quantatity.
 ****************************************************************** */
void Transport_PK::add_dispersive_fluxes(int component,
                                         Teuchos::RCP<Epetra_MultiVector> tcc,
                                         std::vector<int>& bc_face_id,
                                         std::vector<double>& bc_face_values,
                                         Teuchos::RCP<Epetra_MultiVector> tcc_next)
{
  WhetStone::MFD3D mfd(mesh_);
  AmanziMesh::Entity_ID_List nodes, faces;
  std::vector<AmanziGeometry::Point> corner_points;
  std::vector<double> corner_values, corner_fluxes;

  for (int c=cmin; c<cmax_owned; c++) {
    mesh_->cell_get_nodes(c, &nodes);
    int nnodes = nodes.size();
    double value = (*tcc)[component][c];

    for (int n=0; n<nnodes; n++) {
      int v = nodes[n];
      mesh_->node_get_cell_faces(v, c, AmanziMesh::USED, &faces);
      int nfaces = faces.size();

      corner_points.clear();
      corner_values.clear();
      for (int i=0; i<nfaces; i++) {
        int f = faces[i];
        corner_points.push_back(harmonic_points[f]);
        corner_values.push_back(harmonic_points_value[f]);
      }

      mfd.dispersion_corner_fluxes(
          v, c, dispersion_tensor[c], corner_points, value, corner_values, corner_fluxes);

      for (int i=0; i<nfaces; i++) {
        int f = faces[i];
        if (bc_face_id[f] == TRANSPORT_BC_DISPERSION_FLUX) {
          corner_fluxes[i] = bc_face_values[i];
        }

        (*tcc_next)[component][c] += corner_fluxes[i];
        int c2 = mfd.cell_get_face_adj_cell(c, f);
        if (c2 >= 0) (*tcc_next)[component][c2] -= corner_fluxes[i];
      }
    }
  }
}


/* *******************************************************************
 * Identify flux direction based on orientation of the face normal 
 * and sign of the  Darcy velocity.                               
 ****************************************************************** */
void Transport_PK::identify_upwind_cells()
{
  Teuchos::RCP<AmanziMesh::Mesh> mesh = TS->get_mesh_maps();
 
  for (int f=fmin; f<=fmax; f++) {
    (*upwind_cell_)[f] = -1;  // negative value is indicator of a boundary
    (*downwind_cell_)[f] = -1;
  }

  AmanziMesh::Entity_ID_List faces; 
  std::vector<int> fdirs;
  Epetra_Vector& darcy_flux = TS_nextBIG->ref_darcy_flux();

  for (int c=cmin; c<=cmax; c++) {
    mesh->cell_get_faces(c, &faces);
    mesh->cell_get_face_dirs(c, &fdirs);

    for (int i=0; i<faces.size(); i++) {
      int f = faces[i];
      if (darcy_flux[f] * fdirs[i] >= 0) { 
        (*upwind_cell_)[f] = c; 
      } else { 
        (*downwind_cell_)[f] = c; 
      }
    }
  }
}

}  // namespace AmanziTransport
}  // namespace Amanzi

