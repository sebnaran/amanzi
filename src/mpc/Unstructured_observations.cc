/*
  This is the flow component of the Amanzi code. 

  Copyright 2010-201x held jointly by LANS/LANL, LBNL, and PNNL. 
  Amanzi is released under the three-clause BSD License. 
  The terms of use and "as is" disclaimer for this license are 
  provided in the top-level COPYRIGHT file.

  Authors: Markus Berndt  
*/

#include <map>

#include "dbc.hh"
#include "errors.hh"
#include "exceptions.hh"
#include "PolygonRegion.hh"
#include "PlaneRegion.hh"

#include "Unstructured_observations.hh"

namespace Amanzi {

/* ******************************************************************
* Constructor.
****************************************************************** */
Unstructured_observations::Unstructured_observations(Teuchos::ParameterList obs_list,
                                                     Amanzi::ObservationData& observation_data,
                                                     Epetra_MpiComm* comm)
    : observation_data_(observation_data), obs_list_(obs_list)
{
  rank_ = comm->MyPID();

  Teuchos::ParameterList tmp_list;
  tmp_list.set<std::string>("Verbosity Level", "high");
  vo_ = new VerboseObject("Observations", tmp_list);

  // loop over the sublists and create an observation for each
  for (Teuchos::ParameterList::ConstIterator i = obs_list_.begin(); i != obs_list_.end(); i++) {

    if (obs_list_.isSublist(obs_list_.name(i))) {
      Teuchos::ParameterList observable_plist = obs_list_.sublist(obs_list_.name(i));

      std::vector<double> times;
      std::vector<std::vector<double> > time_sps;

      // get the observation times
      if (observable_plist.isSublist("time start period stop")) {
        Teuchos::ParameterList& tsps_list = observable_plist.sublist("time start period stop");
        for (Teuchos::ParameterList::ConstIterator it = tsps_list.begin(); it != tsps_list.end(); ++it) {
          std::string name = it->first;
          if (tsps_list.isSublist(name)) {
            Teuchos::ParameterList& itlist = tsps_list.sublist(name);
            if (itlist.isParameter("start period stop")) {
              Teuchos::Array<double> sps = itlist.get<Teuchos::Array<double> >("start period stop");
              time_sps.push_back(sps.toVector());
            }
          }
        }
      }
      if (observable_plist.isParameter("times")) {
        Teuchos::Array<double> vtimes = observable_plist.get<Teuchos::Array<double> >("times");
        times = vtimes.toVector();
      }

      std::vector<int> cycles;
      std::vector<std::vector<int> > cycle_sps;

      // get the observation cycles
      if (observable_plist.isSublist("cycle start period stop")) {
        Teuchos::ParameterList& csps_list = observable_plist.sublist("cycle start period stop");
        for (Teuchos::ParameterList::ConstIterator it = csps_list.begin(); it != csps_list.end(); ++it) {
          std::string name = it->first;
          if (csps_list.isSublist(name)) {
            Teuchos::ParameterList& itlist = csps_list.sublist(name);
            if (itlist.isParameter("start period stop")) {
              Teuchos::Array<int> csps = itlist.get<Teuchos::Array<int> >("start period stop");
              cycle_sps.push_back(csps.toVector());
            }
          }
        }
      }

      if (observable_plist.isParameter("cycles")) {
        Teuchos::Array<int> vcycles = observable_plist.get<Teuchos::Array<int> >("cycles");
        cycles = vcycles.toVector();
      }

      // loop over all variables listed and create an observable for each
      std::string var = observable_plist.get<std::string>("variable");
      observations.insert(std::pair
                          <std::string, Observable>(obs_list_.name(i),
                                                    Observable(var,
                                                               observable_plist.get<std::string>("region"),
                                                               observable_plist.get<std::string>("functional"),
                                                               observable_plist,
                                                               comm)));
    }
  }
}


/* ******************************************************************
* Process data to extract observations.
****************************************************************** */
void Unstructured_observations::MakeObservations(State& state)
{
  Errors::Message msg;

  // loop over all observables
  for (std::map<std::string, Observable>::iterator i = observations.begin(); i != observations.end(); i++) {
    
    if ((i->second).DumpRequested(state.time()) || (i->second).DumpRequested(state.cycle())) {
      
      // for now we can only observe Integrals and Values
      if ((i->second).functional != "Observation Data: Integral"  &&
          (i->second).functional != "Observation Data: Point" )  {
        msg << "Unstructured_observations: can only handle Functional == Observation Data:"
            << " Integral, or Functional == Observation Data: Point";
        Exceptions::amanzi_throw(msg);
      }
      
      std::string label = i->first;
      
      // we need to make an observation for each variable in the observable
      std::string var = (i->second).variable;
      
      // data structure to store the observation
      Amanzi::ObservationData::DataTriple data_triplet;
      
      // build the name of the observation
      std::stringstream ss;
      ss << label << ", " << var;
      
      std::vector<Amanzi::ObservationData::DataTriple>& od = observation_data_[label]; 

      // check if observation is planar
      bool obs_planar(false);

      AmanziGeometry::GeometricModelPtr gm_ptr = state.GetMesh()->geometric_model();
      AmanziGeometry::RegionPtr reg_ptr = gm_ptr->FindRegion((i->second).region);
      AmanziGeometry::Point reg_normal;
      if (reg_ptr->type() == AmanziGeometry::POLYGON) {
        AmanziGeometry::PolygonRegion *poly_reg = dynamic_cast<AmanziGeometry::PolygonRegion*>(reg_ptr);
        reg_normal = poly_reg->normal();
        obs_planar = true;
      } else if (reg_ptr->type() == AmanziGeometry::PLANE) {
        AmanziGeometry::PlaneRegion *plane_reg = dynamic_cast<AmanziGeometry::PlaneRegion*>(reg_ptr);
        reg_normal = plane_reg->normal();
        obs_planar = true;
      }

      // check if observation of solute was requested
      bool obs_solute(false), obs_aqueous(true);     
      int tcc_index(-1);
      for (tcc_index = 0; tcc_index != comp_names_.size(); ++tcc_index) {
        int pos = var.find(comp_names_[tcc_index]);
        if (pos == 0) { 
          obs_solute = true;
          obs_aqueous = false;
          break;
        }
      }

      // check if observation is on faces of cells. 
      bool obs_boundary(false);
      unsigned int mesh_block_size(0);
      Amanzi::AmanziMesh::Entity_ID_List entity_ids;
      std::string solute_var;
      if (obs_solute) solute_var = comp_names_[tcc_index] + " volumetric flow rate";
      if (var == "Aqueous mass flow rate" || 
          var == "Aqueous volumetric flow rate" ||
          var == solute_var) {  // flux needs faces
        mesh_block_size = state.GetMesh()->get_set_size((i->second).region,
                                                        Amanzi::AmanziMesh::FACE,
                                                        Amanzi::AmanziMesh::OWNED);
        entity_ids.resize(mesh_block_size);
        state.GetMesh()->get_set_entities((i->second).region, 
                                          Amanzi::AmanziMesh::FACE, Amanzi::AmanziMesh::OWNED,
                                          &entity_ids);
        obs_boundary = true;
        for (int i = 0; i != mesh_block_size; ++i) {
          int f = entity_ids[i];
          Amanzi::AmanziMesh::Entity_ID_List cells;
          state.GetMesh()->face_get_cells(f, Amanzi::AmanziMesh::USED, &cells);
          if (cells.size() == 2) {
            obs_boundary = false;
            break;
          }
        }
      } else { // all others need cells
        mesh_block_size = state.GetMesh()->get_set_size((i->second).region,
                                                        Amanzi::AmanziMesh::CELL,
                                                        Amanzi::AmanziMesh::OWNED);    
        entity_ids.resize(mesh_block_size);
        state.GetMesh()->get_set_entities((i->second).region,
                                          Amanzi::AmanziMesh::CELL, Amanzi::AmanziMesh::OWNED,
                                          &entity_ids);
      }
      
      // find global meshblocksize
      int dummy = mesh_block_size; 
      int global_mesh_block_size(0);
      state.GetMesh()->get_comm()->SumAll(&dummy, &global_mesh_block_size, 1);
      
      if (global_mesh_block_size == 0) {  // bail if this region is empty
        Teuchos::OSTab tab = vo_->getOSTab();
        *vo_->os() << "Cannot make observation on region \"" << (i->second).region 
                   << "\", that is empty or has incorrect face/cell type, skipping it." << std::endl;
        continue;
      }

      double value(0.0), volume(0.0);

      // the user is asking to for an observation on tcc
      if (obs_solute) { 
        const Epetra_MultiVector& tcc = 
            *state.GetFieldData("total_component_concentration")->ViewComponent("cell", false);

        if (var == comp_names_[tcc_index] + " Aqueous concentration") { 
          for (int i = 0; i < mesh_block_size; i++) {
            int c = entity_ids[i];
            value += tcc[tcc_index][c] * state.GetMesh()->cell_volume(c);
            volume += state.GetMesh()->cell_volume(c);
          }
        } else if (var == comp_names_[tcc_index] + " volumetric flow rate") {
          const Epetra_MultiVector& darcy_flux = *state.GetFieldData("darcy_flux")->ViewComponent("face");
          Amanzi::AmanziMesh::Entity_ID_List cells;

          if (obs_boundary) { // observation is on a boundary set
            for (int i = 0; i != mesh_block_size; ++i) {
              int f = entity_ids[i];
              state.GetMesh()->face_get_cells(f, Amanzi::AmanziMesh::USED, &cells);

              int sign, c = cells[0];
              const AmanziGeometry::Point& face_normal = state.GetMesh()->face_normal(f, false, c, &sign);
              double area = state.GetMesh()->face_area(f);

              value += std::max(0.0, sign * darcy_flux[0][f]) * tcc[tcc_index][c];
              volume += area;
            }
          } else if (obs_planar) {  // observation is on an interior planar set
            for (int i = 0; i != mesh_block_size; ++i) {
              int f = entity_ids[i];
              state.GetMesh()->face_get_cells(f, Amanzi::AmanziMesh::USED, &cells);

              int csign, c = cells[0];
              const AmanziGeometry::Point& face_normal = state.GetMesh()->face_normal(f, false, c, &csign);
              if (darcy_flux[0][f] * csign < 0) c = cells[1];

              double area = state.GetMesh()->face_area(f);
              double sign = (reg_normal * face_normal) * csign / area;
    
              value += sign * darcy_flux[0][f] * tcc[tcc_index][c];
              volume += area;
            }
          } else {
            msg << "Observations of \"SOLUTE volumetric flow rate\""
                << " is only possible for Polygon, Plane and Boundary side sets";
            Exceptions::amanzi_throw(msg);
          }
        } else {
          msg << "Cannot make an observation for solute variable \"" << var << "\"";
          Exceptions::amanzi_throw(msg);
        }
      }

      // aqueous observations
      if (obs_aqueous) {
        double rho = *state.GetScalarData("fluid_density");
        const Epetra_MultiVector& porosity = *state.GetFieldData("porosity")->ViewComponent("cell");    
        const Epetra_MultiVector& ws = *state.GetFieldData("saturation_liquid")->ViewComponent("cell");
        const Epetra_MultiVector& pressure = *state.GetFieldData("pressure")->ViewComponent("cell");
  
        if (var == "Volumetric water content") {
          for (int i = 0; i < mesh_block_size; i++) {
            int c = entity_ids[i];
            double vol = state.GetMesh()->cell_volume(c);
            volume += vol;
            value += porosity[0][c] * ws[0][c] * vol;
          }
        } else if (var == "Gravimetric water content") {
          double particle_density(1.0);  // does not exist in new state, yet... TODO
  
          for (int i = 0; i < mesh_block_size; i++) {
            int c = entity_ids[i];
            double vol = state.GetMesh()->cell_volume(c);
            volume += vol;
            value += porosity[0][c] * ws[0][c] * rho / (particle_density * (1.0 - porosity[0][c])) * vol;
          }    
        } else if (var == "Aqueous pressure") {
          for (int i = 0; i < mesh_block_size; i++) {
            int c = entity_ids[i];
            double vol = state.GetMesh()->cell_volume(c);
            volume += vol;
            value += pressure[0][c] * vol;
          }
        } else if (var == "Aqueous saturation") {
          for (int i = 0; i < mesh_block_size; i++) {
            int c = entity_ids[i];
            double vol = state.GetMesh()->cell_volume(c);
            volume += vol;
            value += ws[0][c] * vol;
          }    
        } else if (var == "Hydraulic Head") {
          const Epetra_MultiVector& hydraulic_head = *state.GetFieldData("hydraulic_head")->ViewComponent("cell");
  
          for (int i = 0; i < mesh_block_size; ++i) {
            int c = entity_ids[i];
            double vol = state.GetMesh()->cell_volume(c);
            volume += vol;
            value += hydraulic_head[0][c] * vol;
          }
        } else if (var == "Drawdown") {
          const Epetra_MultiVector& hydraulic_head = *state.GetFieldData("hydraulic_head")->ViewComponent("cell");
  
          for (int i = 0; i < mesh_block_size; ++i) {
            int c = entity_ids[i];
            double vol = state.GetMesh()->cell_volume(c);
            volume += vol;
            value += hydraulic_head[0][c] * vol;
          }

          std::map<std::string, double>::iterator it = drawdown_.find(label);
          if (it == drawdown_.end()) { 
            drawdown_[label] = value;
            value = 0.0;
          } else {
            value = it->second - value;
          }
        } else if (var == "Aqueous mass flow rate" || 
                   var == "Aqueous volumetric flow rate") {
          double density(1.0);
          if (var == "Aqueous mass flow rate") density = rho;
          const Epetra_MultiVector& darcy_flux = *state.GetFieldData("darcy_flux")->ViewComponent("face");
  
          if (obs_boundary) { // observation is on a boundary set
            Amanzi::AmanziMesh::Entity_ID_List cells;

            for (int i = 0; i != mesh_block_size; ++i) {
              int f = entity_ids[i];
              state.GetMesh()->face_get_cells(f, Amanzi::AmanziMesh::USED, &cells);

              int sign, c = cells[0];
              const AmanziGeometry::Point& face_normal = state.GetMesh()->face_normal(f, false, c, &sign);
              double area = state.GetMesh()->face_area(f);

              value += sign * darcy_flux[0][f] * density;
              volume += area;
            }
          } else if (obs_planar) {  // observation is on an interior planar set
            for (int i = 0; i != mesh_block_size; ++i) {
              int f = entity_ids[i];
              const AmanziGeometry::Point& face_normal = state.GetMesh()->face_normal(f);
              double area = state.GetMesh()->face_area(f);
              double sign = reg_normal * face_normal / area;
    
              value += sign * darcy_flux[0][f] * density;
              volume += area;
            }
          } else {
            msg << "Observations of \"Aqueous mass flow rate\" and \"Aqueous volumetric flow rate\""
                << " are only possible for Polygon, Plane and Boundary side sets";
            Exceptions::amanzi_throw(msg);
          }
        } else {
          msg << "Cannot make an observation for aqueous variable \"" << var << "\"";
          Exceptions::amanzi_throw(msg);
        }
      }
      
      // syncronize the result across processors
      double result;
      state.GetMesh()->get_comm()->SumAll(&value, &result, 1);
      
      double vresult;
      state.GetMesh()->get_comm()->SumAll(&volume, &vresult, 1);
 
      if ((i->second).functional == "Observation Data: Integral") {  
        data_triplet.value = result;  
      } else if ((i->second).functional == "Observation Data: Point") {
        data_triplet.value = result/vresult;
      }
      
      data_triplet.is_valid = true;
      data_triplet.time = state.time();
      
      od.push_back(data_triplet);
    }
  }

  FlushObservations();
}


/* ******************************************************************
* Save observation based on time or cycle.
****************************************************************** */
bool Unstructured_observations::DumpRequested(const double time) {
  bool result = false;
  for (std::map<std::string, Observable>::iterator i = observations.begin(); i != observations.end(); i++) {
    result = result || (i->second).DumpRequested(time);
  }  
  return result;
}


bool Unstructured_observations::DumpRequested(const int cycle) {
  bool result = false;
  for (std::map<std::string, Observable>::iterator i = observations.begin(); i != observations.end(); i++) {
    result = result || (i->second).DumpRequested(cycle);
  }  
  return result;  
}


bool Unstructured_observations::DumpRequested(const int cycle, const double time) {
  return DumpRequested(time) || DumpRequested(cycle);
}


/* ******************************************************************
* Loop over all observations and register each of them with the time 
* step manager.
****************************************************************** */
void Unstructured_observations::RegisterWithTimeStepManager(const Teuchos::Ptr<TimeStepManager>& tsm) {
  for (std::map<std::string, Observable>::iterator i = observations.begin(); i != observations.end(); i++) {
    (i->second).RegisterWithTimeStepManager(tsm);
  }  
}


/* ******************************************************************
* Write observatins to a file. Clsoe the file to flush data.
****************************************************************** */
void Unstructured_observations::FlushObservations()
{
  if (obs_list_.isParameter("Observation Output Filename")) {
    std::string obs_file = obs_list_.get<std::string>("Observation Output Filename");
    int precision = obs_list_.get<int>("precision", 16);

    if (rank_ == 0) {
      std::ofstream out;
      out.open(obs_file.c_str(),std::ios::out);
      
      out.precision(precision);
      out.setf(std::ios::scientific);

      out << "Observation Name, Region, Functional, Variable, Time, Value\n";
      out << "===========================================================\n";

      for (Teuchos::ParameterList::ConstIterator i = obs_list_.begin(); i != obs_list_.end(); ++i) {
        std::string label = obs_list_.name(i);
        const Teuchos::ParameterEntry& entry = obs_list_.getEntry(label);
        if (entry.isList()) {
          const Teuchos::ParameterList& ind_obs_list = obs_list_.sublist(label);

          for (int j = 0; j < observation_data_[label].size(); j++) {
            if (observation_data_[label][j].is_valid) {
              if (!out.good()) {
                std::cout << "PROBLEM BEFORE" << std::endl;
              }
              out << label << ", "
                  << ind_obs_list.get<std::string>("region") << ", "
                  << ind_obs_list.get<std::string>("functional") << ", "
                  << ind_obs_list.get<std::string>("variable") << ", "
                  << observation_data_[label][j].time << ", "
                  << observation_data_[label][j].value << '\n';
              if (!out.good()) {
                std::cout << "PROBLEM AFTER" << std::endl;
              }
            }
          }
        }
      }
      out.close();
    }
  }
}

}  // namespace Amanzi
