/*
  MPC PK

  Copyright 2010-201x held jointly by LANS/LANL, LBNL, and PNNL. 
  Amanzi is released under the three-clause BSD License. 
  The terms of use and "as is" disclaimer for this license are 
  provided in the top-level COPYRIGHT file.

  Registration of MPC PKs.
*/

#ifndef DISABLE_PHYSICS
#include "FlowEnergy_PK.hh"
#include "FlowMatrixFracture_PK.hh"
#include "FlowReactiveTransport_PK.hh"
#endif

#include "ReactiveTransport_PK.hh"
#include "TransportMatrixFracture_PK.hh"
#include "TransportMatrixFractureImplicit_PK.hh"

namespace Amanzi {

#ifndef DISABLE_PHYSICS
RegisteredPKFactory<FlowEnergy_PK> FlowEnergy_PK::reg_("thermal richards");
RegisteredPKFactory<FlowReactiveTransport_PK> FlowReactiveTransport_PK::reg_("flow reactive transport");
RegisteredPKFactory<FlowMatrixFracture_PK> FlowMatrixFracture_PK::reg_("darcy matrix fracture");
#endif

RegisteredPKFactory<ReactiveTransport_PK> ReactiveTransport_PK::reg_("reactive transport");

RegisteredPKFactory<TransportMatrixFracture_PK> TransportMatrixFracture_PK::reg_("transport matrix fracture");  
RegisteredPKFactory<TransportMatrixFractureImplicit_PK> TransportMatrixFractureImplicit_PK::reg_("transport matrix fracture implicit");  

}  // namespace Amanzi

