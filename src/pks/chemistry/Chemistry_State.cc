#include "Chemistry_State.hh"

namespace Amanzi {
namespace AmanziChemistry {

Chemistry_State::Chemistry_State(Teuchos::ParameterList& plist,
                                 const std::vector<std::string>& component_names,
                                 const Teuchos::RCP<State>& S) :
    S_(S),
    ghosted_(true),
    name_("state"),
    plist_(plist),
    number_of_aqueous_components_(component_names.size()),
    number_of_minerals_(0),
    number_of_ion_exchange_sites_(0),
    number_of_sorption_sites_(0),
    using_sorption_(false),
    using_sorption_isotherms_(false),
    compnames_(component_names),
    num_aux_data_(-1) {

  mesh_ = S_->GetMesh();
  // SetupSoluteNames_();
  SetupMineralNames_();
  SetupSorptionSiteNames_();

  // in the old version, this was only in the Block sublist... may need work?
  if (plist_.isParameter("Cation Exchange Capacity")) {
    using_sorption_ = true;
    number_of_ion_exchange_sites_ = 1;
  }

  ParseMeshBlocks_();
}


void Chemistry_State::Setup() {
  RequireData_();  
  RequireAuxData_();
}


void Chemistry_State::SetupMineralNames_() {
  // do we need to worry about minerals?
  mineral_names_.clear();
  Teuchos::Array<std::string> data;
  if (plist_.isParameter("Minerals")) {
    data = plist_.get<Teuchos::Array<std::string> >("Minerals");
  }

  // the mineral_names_ list should be the order expected by the chemistry....
  mineral_names_.clear();
  mineral_name_id_map_.clear();
  for (int m = 0; m < data.size(); ++m) {
    mineral_name_id_map_[data.at(m)] = m;
    mineral_names_.push_back(data.at(m));
  }

  if (mineral_names_.size() > 0) {
    // we read some mineral names, so override any value that may have
    // been set in the constructor
    number_of_minerals_ = mineral_names_.size();
  }
}


void Chemistry_State::SetupSorptionSiteNames_() {
  // could almost generalize the SetupMineralNames and
  // SetupSorptionSiteNames into a single function w/ different
  // parameters, but using_sorption needs to be set...

  // do we need to worry about sorption sites?
  sorption_site_names_.clear();
  Teuchos::Array<std::string> data;
  if (plist_.isParameter("Sorption Sites")) {
    data = plist_.get<Teuchos::Array<std::string> >("Sorption Sites");
  }

  // the sorption_site_names_ list should be the order expected by the chemistry...
  sorption_site_names_.clear();
  sorption_site_name_id_map_.clear();
  for (int s = 0; s < data.size(); ++s) {
    sorption_site_name_id_map_[data.at(s)] = s;
    sorption_site_names_.push_back(data.at(s));
  }

  if (sorption_site_names_.size() > 0) {
    // we read some sorption site names, so override any value that
    // may have been set in the constructor and set the sorption flag
    // so we allocate the correct amount of memory
    number_of_sorption_sites_ = sorption_site_names_.size();
    using_sorption_ = true;
  } else if (number_of_sorption_sites() > 0 && sorption_site_names_.size() == 0) {
    // assume we are called from the constructor w/o a valid parameter
    // list and the sorption names will be set later....?
  }
}


void Chemistry_State::ParseMeshBlocks_() {
  // check if there is an initial condition for ion_exchange_sites
  if (plist_.sublist("initial conditions").isSublist("ion_exchange_sites")) {
    // there is currently only at most one site...
    using_sorption_ = true;
    number_of_ion_exchange_sites_ = 1;
  }

  if (plist_.sublist("initial conditions").isSublist("isotherm_kd")) {
    using_sorption_ = true;
    using_sorption_isotherms_ = true;
  }

  if (plist_.sublist("initial conditions").isSublist("sorption_sites")) {
    using_sorption_ = true;
    Teuchos::Array<std::string> ss_names_ = plist_.get<Teuchos::Array<std::string> >("Sorption Sites");
    number_of_sorption_sites_ = ss_names_.size();
  }
}


void Chemistry_State::VerifyMineralogy_(const std::string& region_name,
                                        const Teuchos::ParameterList& minerals_list) {
  // loop through each mineral, verify that the mineral name is known
  for (Teuchos::ParameterList::ConstIterator mineral_iter = minerals_list.begin();
       mineral_iter != minerals_list.end(); ++mineral_iter) {
    std::string mineral_name = minerals_list.name(mineral_iter);
    if (!mineral_name_id_map_.count(mineral_name)) {
      std::stringstream message;
      message << "Error: Chemistry_State::VerifyMineralogy(): " << mineral_name
              << " was specified in the mineralogy for region "
              << region_name << " but was not listed in the minerals phase list.\n";
      Exceptions::amanzi_throw(Errors::Message(message.str()));
    }

    // all minerals will have a volume fraction and specific surface
    // area, but sane defaults can be provided, so we don't bother
    // with them here.
  } 
} 


void Chemistry_State::VerifySorptionIsotherms_(const std::string& region_name,
        const Teuchos::ParameterList& isotherms_list) {
  // verify that every species listed is in the component names list.
  // verify that every listed species has a Kd value (no sane default)
  // langmuir and freundlich values are optional (sane defaults)
  using_sorption_ = true;
  using_sorption_isotherms_ = true;

  // loop through each species in the isotherm list
  for (Teuchos::ParameterList::ConstIterator species_iter = isotherms_list.begin();
       species_iter != isotherms_list.end(); ++species_iter) {
    std::string species_name = isotherms_list.name(species_iter);

    // verify that the name is a known species
    if (!comp_name_id_map_.count(species_name)) {
      std::stringstream message;
      message << "Error: Chemistry_State::VerifySorptionIsotherms(): region: "
              << region_name << " contains isotherm data for solute \'"
              << species_name
              << "\' but it is not specified in the component solutes list.\n";
      Exceptions::amanzi_throw(Errors::Message(message.str()));
    }


    // check that this item is a sublist:
    Teuchos::ParameterList species_data;
    if (!isotherms_list.isSublist(species_name)) {
      std::stringstream message;
      message << "Error: Chemistry_State::VerifySorptionIsotherms(): region: "
              << region_name << " ; species : " << species_name
              << " ; must be a named \'ParameterList\' of isotherm data.\n";
      Exceptions::amanzi_throw(Errors::Message(message.str()));
    } else {
      species_data = isotherms_list.sublist(species_name);
    }

    // verify that the required parameters are present
    if (!species_data.isParameter("Kd")) {
      std::stringstream message;
      message << "Error: Chemistry_State::VerifySorptionIsotherms(): region: "
              << region_name << " ; species name: " << species_name
              << " ; each isotherm must have a 'Kd' parameter.\n";
      Exceptions::amanzi_throw(Errors::Message(message.str()));
    }
    // langmuir and freundlich parameters are optional, we'll assign
    // sane defaults.
  }
}


void Chemistry_State::VerifySorptionSites_(const std::string& region_name,
        const Teuchos::ParameterList& sorption_site_list) {
  using_sorption_ = true;
  // loop through each sorption site, verify that the site name is known
  for (Teuchos::ParameterList::ConstIterator site_iter = sorption_site_list.begin();
       site_iter != sorption_site_list.end(); ++site_iter) {
    std::string site_name = sorption_site_list.name(site_iter);
    if (!sorption_site_name_id_map_.count(site_name)) {
      std::stringstream message;
      message << "Error: Chemistry_State::VerifySorptionSites(): " << site_name
              << " was specified in the 'Surface Complexation Sites' list for region "
              << region_name << " but was not listed in the sorption sites phase list.\n";
      Exceptions::amanzi_throw(Errors::Message(message.str()));
    }

    // all sorption sites will have a site density
    // but we can default to zero, so don't do any further checking
  }
}


void Chemistry_State::RequireData_() {
  // Require data from flow
  if (!S_->HasField("porosity")) {
    S_->RequireField("porosity", name_)->SetMesh(mesh_)->SetGhosted(false)
      ->SetComponent("cell", AmanziMesh::CELL, 1);
  }

  if (!S_->HasField("saturation_liquid")) {
    S_->RequireField("saturation_liquid", name_)->SetMesh(mesh_)->SetGhosted(false)
      ->SetComponent("cell", AmanziMesh::CELL, 1);
  }
  
  if (!S_->HasField("fluid_density")) {
    S_->RequireScalar("fluid_density", name_);
  }

  // Require my data
  if (number_of_aqueous_components_ > 0) {
    // Make dummy names if our component names haven't already been set.
    if (compnames_.empty()) {
      for (int i = 0; i < number_of_aqueous_components_; ++i) {
        std::stringstream ss;
        ss << "Component " << i << std::ends;
        compnames_.push_back(ss.str());
      }
    }

    // TCC
    if (!S_->HasField("total_component_concentration")) {    
      // set the names for vis
      std::vector<std::vector<std::string> > conc_names_cv(1);
      for (std::vector<std::string>::const_iterator compname = compnames_.begin();
           compname != compnames_.end(); ++compname) {
        conc_names_cv[0].push_back(*compname + std::string(" conc"));
      }
      S_->RequireField("total_component_concentration", name_, conc_names_cv)
        ->SetMesh(mesh_)->SetGhosted(true)
        ->SetComponent("cell", AmanziMesh::CELL, number_of_aqueous_components_);
    }

    // now create the map
    for (int i=0; i!=number_of_aqueous_components_; ++i) {
      comp_name_id_map_[compnames_[i]] = i;
    }


    // CreateStoragePrimarySpecies()
    {
      std::vector<std::vector<std::string> > species_names_cv(1);
      for (std::vector<std::string>::const_iterator compname = compnames_.begin();
           compname != compnames_.end(); ++compname) {
        species_names_cv[0].push_back(*compname);
      }
      S_->RequireField("free_ion_species", name_, species_names_cv)
        ->SetMesh(mesh_)->SetGhosted(false)
        ->SetComponent("cell", AmanziMesh::CELL, number_of_aqueous_components_);
      S_->RequireField("primary_activity_coeff", name_, species_names_cv)
        ->SetMesh(mesh_)->SetGhosted(false)
        ->SetComponent("cell", AmanziMesh::CELL, number_of_aqueous_components_);
    }

    // CreateStorageTotalSorbed()
    if (using_sorption_) {
      S_->RequireField("total_sorbed", name_)
          ->SetMesh(mesh_)->SetGhosted(false)
          ->SetComponent("cell", AmanziMesh::CELL, number_of_aqueous_components_);
    }

    // CreateStorageSorptionIsotherms()
    if (using_sorption_isotherms_) {
      S_->RequireField("isotherm_kd", name_)
          ->SetMesh(mesh_)->SetGhosted(false)
          ->SetComponent("cell", AmanziMesh::CELL, number_of_aqueous_components_);
      S_->RequireField("isotherm_freundlich_n", name_)
          ->SetMesh(mesh_)->SetGhosted(false)
          ->SetComponent("cell", AmanziMesh::CELL, number_of_aqueous_components_);
      S_->RequireField("isotherm_langmuir_b", name_)
          ->SetMesh(mesh_)->SetGhosted(false)
          ->SetComponent("cell", AmanziMesh::CELL, number_of_aqueous_components_);
    }
  }

  // CreateStorageIonExchange()
  if (number_of_ion_exchange_sites_ > 0) {
    S_->RequireField("ion_exchange_sites", name_)
        ->SetMesh(mesh_)->SetGhosted(false)
        ->SetComponent("cell", AmanziMesh::CELL, number_of_ion_exchange_sites_);
    S_->RequireField("ion_exchange_ref_cation_conc", name_)
        ->SetMesh(mesh_)->SetGhosted(false)
        ->SetComponent("cell", AmanziMesh::CELL, number_of_ion_exchange_sites_);
  }

  // CreateStorageSurfaceComplexation()
  if (number_of_sorption_sites_ > 0) {
    if (sorption_site_names_.size() > 0) {
      // set the names for vis
      ASSERT(sorption_site_names_.size() == number_of_sorption_sites_);
      std::vector<std::vector<std::string> > ss_names_cv(1);
      std::vector<std::vector<std::string> > scfsc_names_cv(1);
      for (std::vector<std::string>::const_iterator sorption_site_name=
               sorption_site_names_.begin();
           sorption_site_name!=sorption_site_names_.end(); ++sorption_site_name) {
        ss_names_cv[0].push_back(*sorption_site_name + std::string(" sorption site"));
        scfsc_names_cv[0].push_back(*sorption_site_name +
                std::string(" surface complex free site conc"));
      }
      S_->RequireField("sorption_sites", name_, ss_names_cv)
          ->SetMesh(mesh_)->SetGhosted(false)
          ->SetComponent("cell", AmanziMesh::CELL, number_of_sorption_sites_);
      S_->RequireField("surface_complex_free_site_conc", name_, scfsc_names_cv)
          ->SetMesh(mesh_)->SetGhosted(false)
          ->SetComponent("cell", AmanziMesh::CELL, number_of_sorption_sites_);
    } else {
      S_->RequireField("sorption_sites", name_)
          ->SetMesh(mesh_)->SetGhosted(false)
          ->SetComponent("cell", AmanziMesh::CELL, number_of_sorption_sites_);
      S_->RequireField("surface_complex_free_site_conc", name_)
          ->SetMesh(mesh_)->SetGhosted(false)
          ->SetComponent("cell", AmanziMesh::CELL, number_of_sorption_sites_);
    }
  }
}


void Chemistry_State::SetAuxDataNames(const std::vector<std::string>& aux_data_names) {
  for (size_t i = 0; i < aux_data_names.size(); ++i) {
    std::vector<std::vector<std::string> > subname(1);
    subname[0].push_back("0");
    if (!S_->HasField(aux_data_names[i])) {
      Teuchos::RCP<CompositeVectorSpace> fac = S_->RequireField(aux_data_names[i], name_, subname);
      fac->SetMesh(mesh_);
      fac->SetGhosted(false);
      fac->SetComponent("cell", AmanziMesh::CELL, 1);
      Teuchos::RCP<CompositeVector> sac = Teuchos::rcp(new CompositeVector(*fac));

      // Zero the field.
      S_->GetField(aux_data_names[i], name_)->SetData(sac);
      S_->GetField(aux_data_names[i], name_)->CreateData();
      S_->GetFieldData(aux_data_names[i], name_)->PutScalar(0.0);
      S_->GetField(aux_data_names[i], name_)->set_initialized();
    }
  }
}

void Chemistry_State::RequireAuxData_() {
  if (plist_.isParameter("auxiliary data"))  {
    Teuchos::Array<std::string> names = plist_.get<Teuchos::Array<std::string> >("auxiliary data");  
    
    for (Teuchos::Array<std::string>::const_iterator name = names.begin(); name != names.end(); ++name) {

      // Insert the field into the state.
      std::vector<std::vector<std::string> > subname(1);
      subname[0].push_back("0");
      S_->RequireField(*name, name_, subname)
          ->SetMesh(mesh_)->SetGhosted(false)
          ->SetComponent("cell", AmanziMesh::CELL, 1);
    }
  }
}


void Chemistry_State::InitializeField_(Teuchos::ParameterList& ic_plist,
    std::string fieldname, bool sane_default, double default_val) {
  // Initialize mineral volume fractions
  // -- first initialize to a default: this should be a valid default if the
  // parameter is optional, and non-valid if it is not.

  if (S_->HasField(fieldname)) {
    if (!S_->GetField(fieldname)->initialized()) {
      S_->GetFieldData(fieldname, name_)->PutScalar(default_val);
      S_->GetField(fieldname, name_)->set_initialized();
    }
  }
}


void Chemistry_State::Initialize() {
  // Most things are initialized through State, but State can only manage that
  // if they are always initialized.  If sane defaults are available, or they
  // can be derived from other initialized quantities, they are initialized
  // here, where we can manage that logic.

  // initialize list
  Teuchos::ParameterList ic_plist = plist_.sublist("initial conditions");

  // Aqueous species:
  if (number_of_aqueous_components_ > 0) {
    if (!S_->GetField("total_component_concentration",name_)->initialized()) {
      InitializeField_(ic_plist, "total_component_concentration", false, 0.0);
    }
    InitializeField_(ic_plist, "free_ion_species", false, 0.0);
    InitializeField_(ic_plist, "primary_activity_coeff", false, 1.0);

    // Sorption sites: all will have a site density, but we can default to zero
    if (using_sorption_) {
      InitializeField_(ic_plist, "total_sorbed", false, 0.0);
    }

    // Sorption isotherms: Kd required, Langmuir and Freundlich optional
    if (using_sorption_isotherms_) {
      InitializeField_(ic_plist, "isotherm_kd", false, -1.0);
      InitializeField_(ic_plist, "isotherm_freundlich_n", false, 1.0);
      InitializeField_(ic_plist, "isotherm_langmuir_b", false, 1.0);
    }
  }

  // Minerals: vol frac and surface areas
  if (number_of_minerals_ > 0) {
    InitializeField_(ic_plist, "mineral_volume_fractions", false, 0.0);
    InitializeField_(ic_plist, "mineral_specific_surface_area", false, 1.0);
  }

  // Ion exchange sites: default to 1
  if (number_of_ion_exchange_sites_ > 0) {
    InitializeField_(ic_plist, "ion_exchange_sites", false, 1.0);
    InitializeField_(ic_plist, "ion_exchange_ref_cation_conc", false, 1.0);
  }

  if (number_of_sorption_sites_ > 0) {
    InitializeField_(ic_plist, "sorption_sites", false, 1.0);
    InitializeField_(ic_plist, "surface_complex_free_site_conc", false, 1.0);
  }

  // initialize auxiliary fields
  if (plist_.isParameter("auxiliary data"))  {
    Teuchos::Array<std::string> names = plist_.get<Teuchos::Array<std::string> >("auxiliary data");  
    
    for (Teuchos::Array<std::string>::const_iterator name = names.begin(); name != names.end(); ++name) {
      S_->GetFieldData(*name, name_)->PutScalar(0.0);
      S_->GetField(*name, name_)->set_initialized();
    }  
  }
}


// This can only be done AFTER the chemistry is initialized and fully set up?
void Chemistry_State::AllocateAdditionalChemistryStorage(
    const Beaker::BeakerComponents& components) {
  int n_secondary_comps = components.secondary_activity_coeff.size();
  if (n_secondary_comps > 0) {
    // CreateStorageSecondaryActivityCoeff()
    Teuchos::RCP<CompositeVectorSpace> fac =
        S_->RequireField("secondary_activity_coeff", name_);
    fac->SetMesh(mesh_)->SetGhosted(false)
        ->SetComponent("cell", AmanziMesh::CELL, n_secondary_comps);
    Teuchos::RCP<CompositeVector> sac = Teuchos::rcp(new CompositeVector(*fac));
    S_->GetField("secondary_activity_coeff",name_)->SetData(sac);
    S_->GetField("secondary_activity_coeff",name_)->CreateData();
    S_->GetFieldData("secondary_activity_coeff",name_)->PutScalar(1.0);
    S_->GetField("secondary_activity_coeff",name_)->set_initialized();
  }
}


#ifdef ALQUIMIA_ENABLED
void Chemistry_State::CopyToAlquimia(const int cell_id,
                                     AlquimiaMaterialProperties& mat_props,
                                     AlquimiaState& state,
                                     AlquimiaAuxiliaryData& aux_data)
{
  Teuchos::RCP<const Epetra_MultiVector> tcc =
      S_->GetFieldData("total_component_concentration")->ViewComponent("cell");
  CopyToAlquimia(cell_id, tcc, mat_props, state, aux_data);
}

void Chemistry_State::CopyToAlquimia(const int cell_id,
                                     Teuchos::RCP<const Epetra_MultiVector> aqueous_components,
                                     AlquimiaMaterialProperties& mat_props,
                                     AlquimiaState& state,
                                     AlquimiaAuxiliaryData& aux_data)
{
  const Epetra_MultiVector& porosity = *S_->GetFieldData("porosity")->ViewComponent("cell");
  double water_density = *S_->GetScalarData("fluid_density");

  state.water_density = water_density;
  state.porosity = porosity[0][cell_id];

  for (int c = 0; c < number_of_aqueous_components(); c++) {
    state.total_mobile.data[c] = (*aqueous_components)[c][cell_id];
    if (using_sorption()) {
      const Epetra_MultiVector& sorbed = *S_->GetFieldData("total_sorbed")->ViewComponent("cell");
      state.total_immobile.data[c] = sorbed[c][cell_id];
    } 
  }

  // minerals
  assert(state.mineral_volume_fraction.size == number_of_minerals_);
  assert(state.mineral_specific_surface_area.size == number_of_minerals_);

  if (number_of_minerals_ > 0) {
    const Epetra_MultiVector& mineral_vf = *S_->GetFieldData("mineral_volume_fractions")->ViewComponent("cell");
    const Epetra_MultiVector& mineral_ssa = *S_->GetFieldData("mineral_specific_surface_area")->ViewComponent("cell");
    for (unsigned int i = 0; i < number_of_minerals_; ++i) {
      state.mineral_volume_fraction.data[i] = mineral_vf[i][cell_id];
      state.mineral_specific_surface_area.data[i] = mineral_ssa[i][cell_id];
    }
  }

  // ion exchange
  assert(state.cation_exchange_capacity.size == number_of_ion_exchange_sites());
  if (number_of_ion_exchange_sites() > 0) {
    const Epetra_MultiVector& ion_exchange = *S_->GetFieldData("ion_exchange_sites")->ViewComponent("cell");
    for (int i = 0; i < number_of_ion_exchange_sites(); i++) {
      state.cation_exchange_capacity.data[i] = ion_exchange[i][cell_id];
    }
  }
  
  // surface complexation
  if (number_of_sorption_sites() > 0) {
    const Epetra_MultiVector& sorption_sites = *S_->GetFieldData("sorption_sites")->ViewComponent("cell");

    assert(number_of_sorption_sites() == state.surface_site_density.size);
    for (int i = 0; i < number_of_sorption_sites(); ++i) {
      // FIXME: Need site density names, too?
      state.surface_site_density.data[i] = sorption_sites[i][cell_id];
      // TODO(bandre): need to save surface complexation free site conc here!
    }
  }

  // Auxiliary data -- block copy.
  if (S_->HasField("alquimia_aux_data"))
    aux_data_ = S_->GetField("alquimia_aux_data", name_)->GetFieldData()->ViewComponent("cell");
  if (num_aux_data_ != -1) {
    int num_aux_ints = aux_data.aux_ints.size;
    int num_aux_doubles = aux_data.aux_doubles.size;
    for (int i = 0; i < num_aux_ints; i++) {
      double* cell_aux_ints = (*aux_data_)[i];
      aux_data.aux_ints.data[i] = (int)cell_aux_ints[cell_id];
    }
    for (int i = 0; i < num_aux_doubles; i++) {
      double* cell_aux_doubles = (*aux_data_)[i + num_aux_ints];
      aux_data.aux_doubles.data[i] = cell_aux_doubles[cell_id];
    }
  }

  const Epetra_MultiVector& water_saturation = *S_->GetFieldData("saturation_liquid")->ViewComponent("cell");

  mat_props.volume = mesh_->cell_volume(cell_id);
  mat_props.saturation = water_saturation[0][cell_id];

  // sorption isotherms
  if (using_sorption_isotherms()) {
    const Epetra_MultiVector& isotherm_kd = *S_->GetFieldData("isotherm_kd")->ViewComponent("cell");
    const Epetra_MultiVector& isotherm_freundlich_n = *S_->GetFieldData("isotherm_freundlich_n")->ViewComponent("cell");
    const Epetra_MultiVector& isotherm_langmuir_b = *S_->GetFieldData("isotherm_langmuir_b")->ViewComponent("cell");

    for (unsigned int i = 0; i < number_of_aqueous_components(); ++i) {
      mat_props.isotherm_kd.data[i] = isotherm_kd[i][cell_id];
      mat_props.freundlich_n.data[i] = isotherm_freundlich_n[i][cell_id];
      mat_props.langmuir_b.data[i] = isotherm_langmuir_b[i][cell_id];
    }
  }
}


void Chemistry_State::CopyFromAlquimia(const int cell_id,
                                       const AlquimiaMaterialProperties& mat_props,
                                       const AlquimiaState& state,
                                       const AlquimiaAuxiliaryData& aux_data,
                                       const AlquimiaAuxiliaryOutputData& aux_output,
                                       Teuchos::RCP<const Epetra_MultiVector> aqueous_components)
{
  // If the chemistry has modified the porosity and/or density, it needs to 
  // be updated here.
  //(this->water_density())[cell_id] = state.water_density;
  //(this->porosity())[cell_id] = state.porosity;
  for (int c = 0; c < number_of_aqueous_components(); c++) {
    (*aqueous_components)[c][cell_id] = state.total_mobile.data[c];
    if (using_sorption()) {
      const Epetra_MultiVector& sorbed = *S_->GetFieldData("total_sorbed")->ViewComponent("cell");
      sorbed[c][cell_id] = state.total_immobile.data[c];
    }
  }

  // Free ion species.
  const Epetra_MultiVector& free_ion = *S_->GetFieldData("free_ion_species")->ViewComponent("cell");
  for (int i = 0; i < number_of_aqueous_components(); ++i) {
    free_ion[i][cell_id] = aux_output.primary_free_ion_concentration.data[i];
  }

  // Mineral properties.
  if (number_of_minerals_ > 0) {
    const Epetra_MultiVector& mineral_vf = *S_->GetFieldData("mineral_volume_fractions")->ViewComponent("cell");
    const Epetra_MultiVector& mineral_ssa = *S_->GetFieldData("mineral_specific_surface_area")->ViewComponent("cell");

    for (int i = 0; i < number_of_minerals_; ++i) {
      mineral_vf[i][cell_id] = state.mineral_volume_fraction.data[i];
      mineral_ssa[i][cell_id] = state.mineral_specific_surface_area.data[i];
    }
  }

  // ion exchange
  if (number_of_ion_exchange_sites() > 0) {
    const Epetra_MultiVector& ion_exchange = *S_->GetFieldData("ion_exchange_sites")->ViewComponent("cell");
    for (unsigned int i = 0; i < number_of_ion_exchange_sites(); i++) {
      ion_exchange[i][cell_id] = state.cation_exchange_capacity.data[i];
    }
  }

  // surface complexation
  if (number_of_sorption_sites() > 0) {
    const Epetra_MultiVector& sorption_sites = *S_->GetFieldData("sorption_sites")->ViewComponent("cell");

    for (unsigned int i = 0; i < number_of_sorption_sites(); i++) {
      sorption_sites[i][cell_id] = state.surface_site_density.data[i];
    }
  }

  // Auxiliary data -- block copy.
  int num_aux_ints = aux_data.aux_ints.size;
  int num_aux_doubles = aux_data.aux_doubles.size;
  if (num_aux_data_ == -1) 
  {
    // Set things up and register a vector in the State.
    assert(num_aux_ints >= 0);
    assert(num_aux_doubles >= 0);
    num_aux_data_ = num_aux_ints + num_aux_doubles;
    if (!S_->HasField("alquimia_aux_data"))
    {
      Teuchos::RCP<CompositeVectorSpace> fac = S_->RequireField("alquimia_aux_data", name_);
      fac->SetMesh(mesh_);
      fac->SetGhosted(false);
      fac->SetComponent("cell", AmanziMesh::CELL, num_aux_data_);
      Teuchos::RCP<CompositeVector> sac = Teuchos::rcp(new CompositeVector(*fac));

      // Zero the field.
      Teuchos::RCP<Field> F = S_->GetField("alquimia_aux_data", name_);
      F->SetData(sac);
      F->CreateData();
      F->GetFieldData()->PutScalar(0.0);
      F->set_initialized();
    }
    aux_data_ = S_->GetField("alquimia_aux_data", name_)->GetFieldData()->ViewComponent("cell");
  }
  else
  {
    assert(num_aux_data_ == num_aux_ints + num_aux_doubles);
  }
  for (int i = 0; i < num_aux_ints; i++) 
  {
    double* cell_aux_ints = (*aux_data_)[i];
    cell_aux_ints[cell_id] = (double)aux_data.aux_ints.data[i];
  }
  for (int i = 0; i < num_aux_doubles; i++) 
  {
    double* cell_aux_doubles = (*aux_data_)[i + num_aux_ints];
    cell_aux_doubles[cell_id] = aux_data.aux_doubles.data[i];
  }

  if (using_sorption_isotherms()) {
    const Epetra_MultiVector& isotherm_kd = *S_->GetFieldData("isotherm_kd")->ViewComponent("cell");
    const Epetra_MultiVector& isotherm_freundlich_n = *S_->GetFieldData("isotherm_freundlich_n")->ViewComponent("cell");
    const Epetra_MultiVector& isotherm_langmuir_b = *S_->GetFieldData("isotherm_langmuir_b")->ViewComponent("cell");

    for (unsigned int i = 0; i < number_of_aqueous_components(); ++i) {
      isotherm_kd[i][cell_id] = mat_props.isotherm_kd.data[i];
      isotherm_freundlich_n[i][cell_id] = mat_props.freundlich_n.data[i];
      isotherm_langmuir_b[i][cell_id] = mat_props.langmuir_b.data[i];
    }
  }
}
#endif

}  // namespace AmanziChemistry
}  // namespace Amanzi
