/*
  This is the EOS component of the ATS and Amanzi codes.
   
  Copyright 2010-201x held jointly by LANS/LANL, LBNL, and PNNL. 
  Amanzi is released under the three-clause BSD License. 
  The terms of use and "as is" disclaimer for this license are 
  provided in the top-level COPYRIGHT file.

  Authors: Ethan Coon (ecoon@lanl.gov)
  EOS for an ideal gas air with a molar fraction of water vapor.
*/

#ifndef AMANZI_EOS_VAPOR_PRESSURE_RELATION_HH_
#define AMANZI_EOS_VAPOR_PRESSURE_RELATION_HH_

namespace Amanzi {
namespace Relations {

class VaporPressureRelation {
 public:
  virtual ~VaporPressureRelation() {};

  virtual double SaturatedVaporPressure(double T) = 0;
  virtual double DSaturatedVaporPressureDT(double T) = 0;
};

}  // namespace Relations
}  // namespace Amanzi

#endif
