/*
  Chemistry 

  Copyright 2010-201x held jointly by LANS/LANL, LBNL, and PNNL. 
  Amanzi is released under the three-clause BSD License. 
  The terms of use and "as is" disclaimer for this license are 
  provided in the top-level COPYRIGHT file.
*/

#ifndef AMANZI_CHEMISTRY_NEWTON_HH_
#define AMANZI_CHEMISTRY_NEWTON_HH_

#include <iostream>
#include <cmath>
#include <vector>

namespace Amanzi {
namespace AmanziChemistry {

class Newton {
 public:
  explicit Newton(const int n);
  virtual ~Newton() {};

  void LUDecomposition(double** a, int n, int* indx);
  void LUBackSolve(double** a, int n, int* indx, std::vector<double>* b);

  void size(int i) {
    this->size_ = i;
  }
  int size(void) const {
    return this->size_;
  }

  void solve();

 private:

  int size_;
  std::vector<double> x_;
  std::vector<double> r_;

  double d_;
  std::vector<int> indices_;
  std::vector<double> vv_;
};

}  // namespace AmanziChemistry
}  // namespace Amanzi
#endif
