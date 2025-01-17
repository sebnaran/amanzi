/*
  Chemistry 

  Copyright 2010-201x held jointly by LANS/LANL, LBNL, and PNNL. 
  Amanzi is released under the three-clause BSD License. 
  The terms of use and "as is" disclaimer for this license are 
  provided in the top-level COPYRIGHT file.

  Class for surface complexation reaction

  Notes:
  - Each instance of this class should contain a single unique
    surface site (e.g. >FeOH) and ALL surface complexes associated
    with that site!
*/

#ifndef AMANZI_CHEMISTRY_SURFACECOMPLEXATIONRXN_HH_
#define AMANZI_CHEMISTRY_SURFACECOMPLEXATIONRXN_HH_

#include <vector>

#include "surface_complex.hh"
#include "surface_site.hh"

namespace Amanzi {
namespace AmanziChemistry {

// forward declarations from chemistry
class MatrixBlock;

class SurfaceComplexationRxn {
 public:
  SurfaceComplexationRxn();
  SurfaceComplexationRxn(SurfaceSite* surface_sites,
                         const std::vector<SurfaceComplex>& surface_complexes);
  explicit SurfaceComplexationRxn(SurfaceSite surface_sites);
  ~SurfaceComplexationRxn() {};

  // add complexes to the reaction
  void AddSurfaceComplex(SurfaceComplex surface_complex);
  void UpdateSiteDensity(const double);
  double GetSiteDensity(void) const {
    return surface_site_.at(0).molar_density();
  }
  SpeciesId SiteId(void) const {
    return surface_site_.at(0).identifier();
  }

  double free_site_concentration(void) const {
    return surface_site_.at(0).free_site_concentration();
  }

  void set_free_site_concentration(const double value) {
    surface_site_.at(0).set_free_site_concentration(value);
  }

  // update sorbed concentrations
  void Update(const std::vector<Species>& primarySpecies);
  // add stoichiometric contribution of complex to sorbed total
  void AddContributionToTotal(std::vector<double> *total);
  // add derivative of total with respect to free-ion to sorbed dtotal
  void AddContributionToDTotal(const std::vector<Species>& primarySpecies,
                               MatrixBlock* dtotal);
  // If the free site stoichiometry in any of the surface complexes
  // is not equal to 1., we must use Newton's method to solve for
  // the free site concentration.  This function determines if this
  // is the case.
  void SetNewtonSolveFlag(void);

  void display(const Teuchos::RCP<VerboseObject>& vo) const;
  void Display(const Teuchos::RCP<VerboseObject>& vo) const;
  void DisplaySite(const Teuchos::RCP<VerboseObject>& vo) const;
  void DisplayComplexes(const Teuchos::RCP<VerboseObject>& vo) const;
  void DisplayResultsHeader(const Teuchos::RCP<VerboseObject>& vo) const;
  void DisplayResults(const Teuchos::RCP<VerboseObject>& vo) const;

 protected:
  void set_use_newton_solve(const bool b) {
    this->use_newton_solve_ = b;
  };

  bool use_newton_solve(void) const {
    return this->use_newton_solve_;
  };

 private:
  std::vector<SurfaceComplex> surface_complexes_;
  std::vector<SurfaceSite> surface_site_;
  bool use_newton_solve_;

  // std::vector<double> dSx_dmi_;  // temporary storage for derivative calculations
};

}  // namespace AmanziChemistry
}  // namespace Amanzi
#endif
