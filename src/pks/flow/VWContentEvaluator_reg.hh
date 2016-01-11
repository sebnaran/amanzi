/*
  Flow PK 

  Copyright 2010-201x held jointly by LANS/LANL, LBNL, and PNNL. 
  Amanzi is released under the three-clause BSD License. 
  The terms of use and "as is" disclaimer for this license are 
  provided in the top-level COPYRIGHT file.
*/

#include "VWContentEvaluator.hh"

namespace Amanzi {
namespace Flow {

Utils::RegisteredFactory<FieldEvaluator,VWContentEvaluator> VWContentEvaluator::reg_("water content");

}  // namespace Flow
}  // namespace Amanzi
