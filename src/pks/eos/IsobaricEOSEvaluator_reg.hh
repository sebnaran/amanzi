/*
  EOS
   
  Copyright 2010-201x held jointly by LANS/LANL, LBNL, and PNNL. 
  Amanzi is released under the three-clause BSD License. 
  The terms of use and "as is" disclaimer for this license are 
  provided in the top-level COPYRIGHT file.

  Author: Ethan Coon (ecoon@lanl.gov)

  The interface between state/data and the model, an EOS.
*/

#include "EOSFactory.hh"
#include "IsobaricEOSEvaluator.hh"

namespace Amanzi {
namespace EOS {

// registry of method
Utils::RegisteredFactory<FieldEvaluator, IsobaricEOSEvaluator> IsobaricEOSEvaluator::factory_("isobaric eos");

}  // namespace EOS
}  // namespace Amanzi
