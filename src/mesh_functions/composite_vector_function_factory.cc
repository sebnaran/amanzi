/* -*-  mode: c++; c-default-style: "google"; indent-tabs-mode: nil -*- */
/* -------------------------------------------------------------------------
ATS

License: see $ATS_DIR/COPYRIGHT
Author: Ethan Coon

Factory for a CV function on a mesh.
------------------------------------------------------------------------- */

#include "errors.hh"

#include "mesh_function.hh"
#include "MultiFunction.hh"

#include "composite_vector_function_factory.hh"


namespace Amanzi {
namespace Functions {

Teuchos::RCP<CompositeVectorFunction>
CreateCompositeVectorFunction(Teuchos::ParameterList& plist,
        const CompositeVectorSpace& sample) {

  Teuchos::RCP<MeshFunction> mesh_func =
    Teuchos::rcp(new MeshFunction(sample.Mesh()));
  std::vector<std::string> componentname_list;

  // top level plist contains sublists containing the entry
  for (Teuchos::ParameterList::ConstIterator lcv=plist.begin();
       lcv!=plist.end(); ++lcv) {
    std::string name = lcv->first;

    if (plist.isSublist(name)) {
      Teuchos::ParameterList& sublist = plist.sublist(name);

      // grab regions from the sublist
      std::vector<std::string> regions;
      if (sublist.isParameter("region")) {
        if (sublist.isType<std::string>("region")) {
          regions.push_back(sublist.get<std::string>("region"));
        } else {
          Errors::Message msg;
          msg << "CompositeVectorFunctionFactory \"" << plist.name() << "(" << name << ")"
              << "\": parameter \"region\" should be a string.";
          Exceptions::amanzi_throw(msg);
        }
      } else if (sublist.isParameter("regions")) {
        if (sublist.isType<Teuchos::Array<std::string> >("regions")) {
          regions = sublist.get<Teuchos::Array<std::string> >("regions").toVector();
        } else {
          Errors::Message msg;
          msg << "CompositeVectorFunctionFactory \"" << plist.name() << "(" << name << ")"
              << "\": parameter \"regions\" should be an Array(string).";
          Exceptions::amanzi_throw(msg);
        }
      } else {
        Errors::Message msg;
        msg << "CompositeVectorFunctionFactory \"" << plist.name() << "(" << name << ")"
            << "\": parameter \"region\" or \"regions\" must exist.";
        Exceptions::amanzi_throw(msg);
      }

      // grab the name of the components from the list
      std::vector<std::string> components;
      if (sublist.isParameter("component")) {
        if (sublist.isType<std::string>("component")) {
          components.push_back(sublist.get<std::string>("component"));
        } else {
          Errors::Message msg;
          msg << "CompositeVectorFunctionFactory \"" << plist.name() << "(" << name << ")"
              << "\": parameter \"component\" should be a string.";
          Exceptions::amanzi_throw(msg);
        }
      } else if (sublist.isParameter("components")) {
        if (sublist.isType<Teuchos::Array<std::string> >("components")) {
          components = sublist.get<Teuchos::Array<std::string> >("components").toVector();
        } else {
          Errors::Message msg;
          msg << "CompositeVectorFunctionFactory \"" << plist.name() << "(" << name << ")"
              << "\": parameter \"components\" should be an Array(string).";
          Exceptions::amanzi_throw(msg);
        }
      } else {
        Errors::Message msg;
        msg << "CompositeVectorFunctionFactory \"" << plist.name() << "(" << name << ")"
            << "\": parameter \"component\" or \"components\" must exist.";
        Exceptions::amanzi_throw(msg);
      }

      // get the function
      Teuchos::RCP<MultiFunction> func;
      if (sublist.isSublist("function")) {
        Teuchos::ParameterList& func_plist = sublist.sublist("function");
        func = Teuchos::rcp(new MultiFunction(func_plist));
      } else {
        Errors::Message msg;
        msg << "CompositeVectorFunctionFactory \"" << plist.name() << "(" << name << ")"
            << "\": missing \"function\" sublist.";
        Exceptions::amanzi_throw(msg);
      }

      // From the above data, add to the cv function.
      // Loop through components, adding a spec/component name for each.
      for (std::vector<std::string>::const_iterator component=components.begin();
           component!=components.end(); ++component) {

        // get the entity kind based upon the sample vector
        if (!sample.HasComponent(*component)) {
          Errors::Message msg;
          msg << "CompositeVectorFunctionFactory \"" << plist.name() << "(" << name << ")"
              << "\": specified component \"" << *component
              << "\" is either not valid or this vector does not include such a component.";
          Exceptions::amanzi_throw(msg);
        }
        AmanziMesh::Entity_kind kind = sample.Location(*component);

        // -- Create the domain,
        Teuchos::RCP<MeshFunction::Domain> domain =
          Teuchos::rcp(new MeshFunction::Domain(regions, kind));

        // -- and the spec,
        Teuchos::RCP<MeshFunction::Spec> spec =
          Teuchos::rcp(new MeshFunction::Spec(domain, func));

        mesh_func->AddSpec(spec);
        componentname_list.push_back(*component);
      }
    } else {
      Errors::Message msg;
      msg << "CompositeVectorFunctionFactory \"" << plist.name() << "(" << name << ")"
          << "\": is not a sublist.";
      Exceptions::amanzi_throw(msg);
    }
  }

  // create the function
  return Teuchos::rcp(new CompositeVectorFunction(mesh_func,
          componentname_list));
};

} // namespace
} // namespace


