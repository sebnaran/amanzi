/*
This is the Linear Solver component of the Amanzi code. 
License: BSD
Authors: Ethan Coon (ecoon@lanl.gov)
         Konstantin Lipnikov (lipnikov@lanl.gov)

Conjugate gradient method.
Usage: 
*/

#ifndef AMANZI_LINEAR_OPERATOR_HH_
#define AMANZI_LINEAR_OPERATOR_HH_

#include "Teuchos_RCP.hpp"
#include "Teuchos_ParameterList.hpp"

 
namespace Amanzi {
namespace AmanziSolvers {

template<class Matrix, class Vector, class VectorSpace>
class LinearOperator {
 public:
  LinearOperator(Teuchos::RCP<const Matrix> m) : m_(m) {};
  ~LinearOperator() {};

  virtual void Init(Teuchos::ParameterList& plist) = 0;  

  void Apply(const Vector& v, Vector& mv) { m_->Apply(v, mv); }
  virtual void ApplyInverse(const Vector& v, Vector& hv) const = 0;

  Teuchos::RCP<const VectorSpace> domain() const { return m_->domain(); }
  Teuchos::RCP<const VectorSpace> range() const { return m_->range(); }

  double TrueResidual(const Vector& f, const Vector& v) const {
    Vector r(f);
    m_->Apply(v, r);  // r = f - M * x
    r.Update(1.0, f, -1.0);

    double true_residual;
    r.Norm2(&true_residual);
    return true_residual;
  }

  virtual double residual() = 0;
  virtual int num_itrs() = 0;
  std::string& name() { return name_; }

 protected:
  Teuchos::RCP<const Matrix> m_;
  std::string name_;
};

}  // namespace AmanziSolvers
}  // namespace Amanzi
 
#endif
               
