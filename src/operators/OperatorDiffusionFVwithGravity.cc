/*
  Operators 

  Copyright 2010-201x held jointly by LANS/LANL, LBNL, and PNNL. 
  Amanzi is released under the three-clause BSD License. 
  The terms of use and "as is" disclaimer for this license are 
  provided in the top-level COPYRIGHT file.

  Authors: Daniil Svyatskiy (dasvyat@lanl.gov)
           Konstantin Lipnikov (lipnikov@lanl.gov)
*/

#include <vector>
#include <boost/math/tools/roots.hpp>

#include "OperatorDefs.hh"
#include "OperatorDiffusionFVwithGravity.hh"
#include "Op.hh"
#include "Op_SurfaceFace_SurfaceCell.hh"
#include "Op_Face_Cell.hh"
#include "Operator_Cell.hh"

namespace Amanzi {
namespace Operators {

/* ******************************************************************
* This completes initialization of the base class.
****************************************************************** */
void OperatorDiffusionFVwithGravity::InitDiffusion_(Teuchos::ParameterList& plist)
{
  gravity_term_ = Teuchos::rcp(new CompositeVector(*transmissibility_));
}


/* ******************************************************************
* This completes initialization of the base class.
****************************************************************** */
void OperatorDiffusionFVwithGravity::SetDensity(
    const Teuchos::RCP<const CompositeVector>& rho)
{
  OperatorDiffusionWithGravity::SetDensity(rho);
  transmissibility_initialized_ = false;
}


void OperatorDiffusionFVwithGravity::SetDensity(double rho) {
  OperatorDiffusionWithGravity::SetDensity(rho);
  transmissibility_initialized_ = false;
}

  
/* ******************************************************************
* Populate face-based matrices.
****************************************************************** */
void OperatorDiffusionFVwithGravity::UpdateMatrices(
    const Teuchos::Ptr<const CompositeVector>& flux,
    const Teuchos::Ptr<const CompositeVector>& u)
{
  if (!transmissibility_initialized_) ComputeTransmissibility_(gravity_term_);

  OperatorDiffusionFV::UpdateMatrices(flux, u);

  // populating right-hand side
  if (!exclude_primary_terms_) {
    AmanziMesh::Entity_ID_List faces;
    std::vector<int> dirs;

    const std::vector<int>& bc_model = bcs_trial_[0]->bc_model();
    const Epetra_MultiVector& gravity_face = *gravity_term_->ViewComponent("face", true);
    Epetra_MultiVector& rhs_cell = *global_op_->rhs()->ViewComponent("cell");

    // preparing upwind data
    Teuchos::RCP<const Epetra_MultiVector> k_face = Teuchos::null;
    if (k_ != Teuchos::null) {
      if (k_->HasComponent("face")) k_face = k_->ViewComponent("face", true);
    }

    for (int c = 0; c != ncells_owned; ++c) {
      mesh_->cell_get_faces_and_dirs(c, &faces, &dirs);
      int nfaces = faces.size();

      for (int n = 0; n != nfaces; ++n) {
        int f = faces[n];
        if (bc_model[f] == OPERATOR_BC_NEUMANN) continue;
        rhs_cell[0][c] -= dirs[n] * gravity_face[0][f] * (k_face.get() ? (*k_face)[0][f] : 1.0);
      }
    }
  }
}


/* ******************************************************************
* Special implementation of boundary conditions.
****************************************************************** */
void OperatorDiffusionFVwithGravity::ApplyBCs(bool primary, bool eliminate)
{
  OperatorDiffusionFV::ApplyBCs(primary, eliminate);

  Epetra_MultiVector* gravity_face = &*gravity_term_->ViewComponent("face", true);
  const std::vector<int>& bc_model = bcs_trial_[0]->bc_model();

  for (int f = 0; f < nfaces_owned; f++) {
    if (bc_model[f] == OPERATOR_BC_NEUMANN) {
      (*gravity_face)[0][f] = 0.0;
    }
  }
}


/* ******************************************************************
* Calculate flux from cell-centered data
****************************************************************** */
void OperatorDiffusionFVwithGravity::UpdateFlux(
    const CompositeVector& solution, CompositeVector& darcy_mass_flux)
{
  OperatorDiffusionFV::UpdateFlux(solution, darcy_mass_flux);

  // add gravity term
  Epetra_MultiVector& flux = *darcy_mass_flux.ViewComponent("face", false);
  const Teuchos::Ptr<const Epetra_MultiVector> Krel_face =
      k_.get() ? k_->ViewComponent("face", false).ptr() : Teuchos::null;

  if (Krel_face.get()) {
    flux.Multiply(1.0, *Krel_face, *gravity_term_->ViewComponent("face", false), 1.0);
  } else {
    flux.Update(1.0, *gravity_term_->ViewComponent("face", false), 1.0);
  }
}


/* ******************************************************************
* Computation of a local submatrix of the analytical Jacobian 
* (its nonlinear part) on face f.
****************************************************************** */
void OperatorDiffusionFVwithGravity::ComputeJacobianLocal_(
    int mcells, int f, int face_dir, int bc_model_f, double bc_value_f,
    double *pres, double *dkdp_cell, WhetStone::DenseMatrix& Jpp)
{
  const Epetra_MultiVector& trans_face = *transmissibility_->ViewComponent("face", true);
  const Teuchos::Ptr<Epetra_MultiVector> gravity_face =
      gravity_term_.get() ? gravity_term_->ViewComponent("face", true).ptr() : Teuchos::null;

  double dKrel_dp[2];
  double dpres;

  if (mcells == 2) {
    dpres = pres[0] - pres[1];  // + grn;
    if (little_k_ == OPERATOR_LITTLE_K_UPWIND) {
      double flux0to1;
      flux0to1 = trans_face[0][f] * dpres;
      if (gravity_face.get()) flux0to1 += face_dir * (*gravity_face)[0][f];
      if (flux0to1  > OPERATOR_UPWIND_RELATIVE_TOLERANCE) {  // Upwind
        dKrel_dp[0] = dkdp_cell[0];
        dKrel_dp[1] = 0.0;
      } else if (flux0to1 < -OPERATOR_UPWIND_RELATIVE_TOLERANCE) {  // Upwind
        dKrel_dp[0] = 0.0;
        dKrel_dp[1] = dkdp_cell[1];
      } else if (fabs(flux0to1) < OPERATOR_UPWIND_RELATIVE_TOLERANCE) {  // Upwind
        dKrel_dp[0] = 0.5 * dkdp_cell[0];
        dKrel_dp[1] = 0.5 * dkdp_cell[1];
      }
    } else if (little_k_ == OPERATOR_UPWIND_ARITHMETIC_AVERAGE) {
      dKrel_dp[0] = 0.5 * dkdp_cell[0];
      dKrel_dp[1] = 0.5 * dkdp_cell[1];
    } else {
      ASSERT(0);
    }

    Jpp(0, 0) = trans_face[0][f] * dpres * dKrel_dp[0];
    Jpp(0, 1) = trans_face[0][f] * dpres * dKrel_dp[1];

    if (gravity_face.get()) {
      Jpp(0,0) += face_dir * (*gravity_face)[0][f] * dKrel_dp[0];
      Jpp(0,1) += face_dir * (*gravity_face)[0][f] * dKrel_dp[1];
    }
    Jpp(1, 0) = -Jpp(0, 0);
    Jpp(1, 1) = -Jpp(0, 1);

  } else if (mcells == 1) {
    if (bc_model_f == OPERATOR_BC_DIRICHLET) {                   
      pres[1] = bc_value_f;
      dpres = pres[0] - pres[1];  // + grn;
      Jpp(0, 0) = trans_face[0][f] * dpres * dkdp_cell[0];
      if (gravity_face.get())
          Jpp(0,0) += face_dir * (*gravity_face)[0][f] * dkdp_cell[0];
    } else {
      Jpp(0, 0) = 0.0;
    }
  }
}


/* ******************************************************************
* Compute transmissibilities on faces. Requires K, g, rho.
****************************************************************** */
void OperatorDiffusionFVwithGravity::ComputeTransmissibility_(
   Teuchos::RCP<CompositeVector> g_cv)
{
  const Epetra_MultiVector& trans_face = *transmissibility_->ViewComponent("face", true);

  // Compute auxiliary structure. Note that first components of both 
  // fields are used symmetrically (no specific order).
  CompositeVectorSpace cvs;
  cvs.SetMesh(mesh_);
  cvs.SetGhosted(true);
  cvs.SetComponent("face", AmanziMesh::FACE, 1);

  CompositeVector beta(cvs, true);
  Epetra_MultiVector& beta_face = *beta.ViewComponent("face", true);
  beta.PutScalar(0.0);

  CompositeVector h(cvs, true);
  Epetra_MultiVector& h_face = *h.ViewComponent("face", true);
  h.PutScalar(0.0);

  AmanziMesh::Entity_ID_List faces, cells;
  AmanziGeometry::Point a_dist, a;
  WhetStone::Tensor Kc(mesh_->space_dimension(), 1); 
  Kc(0, 0) = 1.0;

  for (int c = 0; c < ncells_owned; ++c) {
    if (K_.get()) Kc = (*K_)[c];
    mesh_->cell_get_faces(c, &faces);
    int nfaces = faces.size();

    for (int i = 0; i < nfaces; i++) {
      int f = faces[i];
      const AmanziGeometry::Point& normal = mesh_->face_normal(f);
      const AmanziGeometry::Point& xf = mesh_->face_centroid(f);
      double area = mesh_->face_area(f);

      a = xf - mesh_->cell_centroid(c);
      double h_tmp = norm(a);
      double s = area / h_tmp;
      double perm = ((Kc * a) * normal) * s;
      double dxn = a * normal;
      h_face[0][f] += h_tmp;
      beta_face[0][f] += fabs(dxn / perm);
    }
  }

  beta.GatherGhostedToMaster(Add);
  h.GatherGhostedToMaster(Add);

  // Compute transmissibilities. Since it is done only, we repeat
  // some calculatons.
  transmissibility_->PutScalar(0.0);

  const Epetra_MultiVector* rho_c = NULL;
  if (!is_scalar_) {
    rho_cv_->ScatterMasterToGhosted("cell");
    rho_c = rho_cv_->ViewComponent("cell",true).get();
  }
  
  for (int f = 0; f < nfaces_owned; f++) {
    mesh_->face_get_cells(f, AmanziMesh::USED, &cells);
    int ncells = cells.size();

    if (ncells == 2) {
      a_dist = mesh_->cell_centroid(cells[1]) - mesh_->cell_centroid(cells[0]);
    } else if (ncells == 1) {    
      const AmanziGeometry::Point& xf = mesh_->face_centroid(f);
      a_dist = xf - mesh_->cell_centroid(cells[0]);
    } 
    a_dist *= 1.0 / norm(a_dist);

    trans_face[0][f] = 1.0 / beta_face[0][f];

    const AmanziGeometry::Point& normal = mesh_->face_normal(f);
    double dir = copysign(1.0, normal * a_dist);

    double rho = is_scalar_ ? rho_ :
      (ncells == 1 ? (*rho_c)[0][cells[0]] : ((*rho_c)[0][cells[0]] + (*rho_c)[0][cells[1]]) / 2.);
    double grav = (g_ * a_dist) * rho * dir;
    grav *= h_face[0][f];

    Epetra_MultiVector& gravity_face = *g_cv->ViewComponent("face", true);
    gravity_face[0][f] = trans_face[0][f] * grav;
  }

#ifdef HAVE_MPI
  transmissibility_->ScatterMasterToGhosted("face", true);
  g_cv->ScatterMasterToGhosted("face", true);
#endif

  transmissibility_initialized_ = true;
}


/* ******************************************************************
* Return value of the gravity flux on the given face f.
****************************************************************** */
double OperatorDiffusionFVwithGravity::ComputeGravityFlux(int f) const
{
  const Epetra_MultiVector& gravity_face = *gravity_term_->ViewComponent("face", true); 
  return gravity_face[0][f];
}

}  // namespace Operators
}  // namespace Amanzi


