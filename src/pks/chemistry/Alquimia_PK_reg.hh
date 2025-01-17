/*
  Chemistry PK

  Copyright 2010-201x held jointly by LANS/LANL, LBNL, and PNNL. 
  Amanzi is released under the three-clause BSD License. 
  The terms of use and "as is" disclaimer for this license are 
  provided in the top-level COPYRIGHT file.

  Amanzi_PK registration
*/

#include "Alquimia_PK.hh"

namespace Amanzi {
namespace AmanziChemistry {

RegisteredPKFactory<Alquimia_PK> Alquimia_PK::reg_("chemistry alquimia");

}  // namespace AmanziChemistry
}  // namespace Amanzi
