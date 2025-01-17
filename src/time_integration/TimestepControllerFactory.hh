/*
  Copyright 2010-201x held jointly by LANS/LANL, LBNL, and PNNL. 
  Amanzi is released under the three-clause BSD License. 
  The terms of use and "as is" disclaimer for this license are 
  provided in the top-level COPYRIGHT file.

  Authors: Ethan Coon (ecoon@lanl.gov)
*/

//! Factory for creating TimestepController objects

/*!

A TimestepController object sets what size timestep to take.  This can be a
variety of things, from fixed timestep size, to adaptive based upon error
control, to adapter based upon simple nonlinear iteration counts.


* `"timestep controller type`" ``[string]`` Set the type.  One of the below types.
* `"timestep controller X parameters`" ``[list]`` List of parameters for a timestep controller of type X.

Available types include:

- TimestepControllerFixed_  (type `"fixed`"), a constant timestep
- TimestepControllerStandard_ (type `'standard`"), an adaptive timestep based upon nonlinear iterations
- TimestepControllerSmarter_ (type `'smarter`"), an adaptive timestep based upon nonlinear iterations with more control
- TimestepControllerAdaptive_ (type `"adaptive`"), an adaptive timestep based upon error control.
- TimestepControllerFromFile_ (type `"from file`"), uses a timestep history loaded from an HDF5 file.  (Usually only used for regression testing.)

*/

#ifndef AMANZI_TS_CONTROLLER_FACTORY_HH_
#define AMANZI_TS_CONTROLLER_FACTORY_HH_

#include "Teuchos_RCP.hpp"
#include "Teuchos_ParameterList.hpp"

#include "errors.hh"
#include "TimestepController.hh"

#include "TimestepControllerFixed.hh"
#include "TimestepControllerStandard.hh"
#include "TimestepControllerSmarter.hh"
#include "TimestepControllerAdaptive.hh"
#include "TimestepControllerFromFile.hh"


namespace Amanzi {

template<class Vector>
struct TimestepControllerFactory {
 public:
  Teuchos::RCP<TimestepController>
  Create(const Teuchos::ParameterList& prec_list,
         Teuchos::RCP<Vector> udot, Teuchos::RCP<Vector> udot_prev);
};


/* ******************************************************************
* Factory of timestep controls.
****************************************************************** */
template<class Vector>
Teuchos::RCP<TimestepController> TimestepControllerFactory<Vector>::Create(
    const Teuchos::ParameterList& slist,
    Teuchos::RCP<Vector> udot, Teuchos::RCP<Vector> udot_prev)
{
  if (slist.isParameter("timestep controller type")) {
    std::string type = slist.get<std::string>("timestep controller type");

    if (type == "fixed") {
      if (!slist.isSublist("timestep controller fixed parameters")) {
        Errors::Message msg("TimestepControllerFactory: missing sublist \"timestep controller fixed parameters\"");
        Exceptions::amanzi_throw(msg);
      }
      Teuchos::ParameterList tslist = slist.sublist("timestep controller fixed parameters");
      return Teuchos::rcp(new TimestepControllerFixed(tslist));

    } else if (type == "standard") {
      if (!slist.isSublist("timestep controller standard parameters")) {
        Errors::Message msg("TimestepControllerFactory: missing sublist \"timestep controller standard parameters\"");
        Exceptions::amanzi_throw(msg);
      }
      Teuchos::ParameterList tslist = slist.sublist("timestep controller standard parameters");
      return Teuchos::rcp(new TimestepControllerStandard(tslist));

    } else if (type == "smarter") {
      if (!slist.isSublist("timestep controller smarter parameters")) {
        Errors::Message msg("TimestepControllerFactory: missing sublist \"timestep controller smarter parameters\"");
        Exceptions::amanzi_throw(msg);
      }
      Teuchos::ParameterList tslist = slist.sublist("timestep controller smarter parameters");
      return Teuchos::rcp(new TimestepControllerSmarter(tslist));

    } else if (type == "adaptive") {
      if (!slist.isSublist("timestep controller adaptive parameters")) {
        Errors::Message msg("TimestepControllerFactory: missing sublist \"timestep controller adaptive parameters\"");
        Exceptions::amanzi_throw(msg);
      }
      Teuchos::ParameterList tslist = slist.sublist("timestep controller adaptive parameters");
      return Teuchos::rcp(new TimestepControllerAdaptive<Vector>(tslist, udot, udot_prev));

    } else if (type == "from file") {
      if (!slist.isSublist("timestep controller from file parameters")) {
        Errors::Message msg("TimestepControllerFactory: missing sublist \"timestep controller from file parameters\"");
        Exceptions::amanzi_throw(msg);
      }
      Teuchos::ParameterList tslist = slist.sublist("timestep controller from file parameters");
      return Teuchos::rcp(new TimestepControllerFromFile(tslist));

    } else {
      Errors::Message msg("TimestepControllerFactory: invalid value of parameter `\"timestep controller type`\"");
      Exceptions::amanzi_throw(msg);
    }
  } else {
    Errors::Message msg("TimestepControllerFactory: parameter `\"timestep controller type`\" is missing");
    Exceptions::amanzi_throw(msg);
  }
  return Teuchos::null;
}

}  // namespace Amanzi

#endif
