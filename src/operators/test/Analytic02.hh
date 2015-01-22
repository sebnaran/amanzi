/*
  This is the operators component of the Amanzi code.

  License: BSD
  Authors: Konstantin Lipnikov (lipnikov@lanl.gov)

  Discrete source operator.
*/

#include "AnalyticBase.hh"

class Analytic02 : public AnalyticBase {
 public:
  Analytic02(Teuchos::RCP<const Amanzi::AmanziMesh::Mesh> mesh) : AnalyticBase(mesh) {};
  ~Analytic02() {};

  Amanzi::WhetStone::Tensor Tensor(const Amanzi::AmanziGeometry::Point& p, double t) {
    Amanzi::WhetStone::Tensor K(2, 2);
    K(0, 0) = 3.0;
    K(1, 1) = 1.0;
    K(0, 1) = 1.0;
    K(1, 0) = 1.0;
    return K;
  }

  double pressure_exact(const Amanzi::AmanziGeometry::Point& p, double t) { 
    double x = p[0];
    double y = p[1];
    return x + 2 * y;
  }

  Amanzi::AmanziGeometry::Point velocity_exact(const Amanzi::AmanziGeometry::Point& p, double t) { 
    Amanzi::AmanziGeometry::Point v(2);
    v[0] = -5.0;
    v[1] = -3.0;
    return v;
  }
 
  Amanzi::AmanziGeometry::Point gradient_exact(const Amanzi::AmanziGeometry::Point& p, double t) { 
    Amanzi::AmanziGeometry::Point v(2);
    v[0] = 1.0;
    v[1] = 2.0;
    return v;
  }

  double source_exact(const Amanzi::AmanziGeometry::Point& p, double t) { 
    return 0.0;
  }
};
