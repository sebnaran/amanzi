/*
  Copyright 2010-201x held jointly by LANS/LANL, LBNL, and PNNL. 
  Amanzi is released under the three-clause BSD License. 
  The terms of use and "as is" disclaimer for this license are 
  provided in the top-level COPYRIGHT file.

  Authors: William Perkins
*/

#include "boost/format.hpp"

#include "Teuchos_CommHelpers.hpp"

#include "MeshFactory.hh"
#include "MeshFileType.hh"
#include "FrameworkTraits.hh"

namespace Amanzi{
namespace AmanziMesh {
  

// -------------------------------------------------------------
//  class MeshFactory
// -------------------------------------------------------------

// -------------------------------------------------------------
// MeshFactory:: constructors / destructor
// -------------------------------------------------------------
MeshFactory::MeshFactory(Comm_ptr_type comm,
                         const Teuchos::RCP<const VerboseObject>& vo)
  : comm_(comm),
    vo_(vo),
    my_preference_(default_preference())
{
}

MeshFactory::~MeshFactory(void)
{
}

// -------------------------------------------------------------
// MeshFactory::preference
// -------------------------------------------------------------
/** 
 * local -- but better be the same on all processes
 *
 * This routine populates the framework preference list, but only
 * with available frameworks.  If none of the preferred frameworks
 * are available, the preference list is left empty and an exception
 * is thrown.
 * 
 * @param pref list of mesh framework preferences
 */
void
MeshFactory::preference(const FrameworkPreference& pref)
{
  my_preference_.clear();
  my_preference_ = available_preference(pref);
  if (my_preference_.empty()) {
    Message e("specified framework(s) not available: ");
    for (FrameworkPreference::const_iterator i = pref.begin(); 
         i != pref.end(); i++) {
      e.add_data(framework_name(*i).c_str());
      e.add_data(" ");
      amanzi_throw(e);
    }
  }
}

// -------------------------------------------------------------
// MeshFactory::create
// -------------------------------------------------------------
/** 
 * Collective
 *
 * This creates a mesh by reading the specified file (or file set).  
 * 
 * @param filename mesh file to read
 * 
 * @return mesh instance
 */
Teuchos::RCP<Mesh> 
MeshFactory::create(const std::string& filename, 
                    const Teuchos::RCP<const AmanziGeometry::GeometricModel>& gm,
                    const bool request_faces, 
                    const bool request_edges)
{
  // check the file format
  Format fmt = file_format(comm_, filename);

  if (fmt == UnknownFormat) {
    FileMessage 
        e(boost::str(boost::format("%s: unknown file format") %
                     filename).c_str());
    amanzi_throw(e);
  }
      
  Message e("MeshFactory::create: error: ");
  int ierr = 0, aerr = 0;

  Teuchos::RCP<Mesh> result;
  for (FrameworkPreference::const_iterator i = my_preference_.begin(); 
       i != my_preference_.end(); i++) {
    if (framework_reads(*i, fmt, comm_->getSize() > 1)) {
      try {
        result = framework_read(comm_, *i, filename, gm, vo_,
                                request_faces, request_edges, partitioner_);
        return result;
      } catch (const Message& msg) {
        ierr += 1;
        e.add_data(msg.what());
      } catch (const std::exception& stde) {
        ierr += 1;
        e.add_data("internal error: ");
        e.add_data(stde.what());
      }
      Teuchos::reduceAll(*comm_, Teuchos::REDUCE_SUM, ierr, Teuchos::outArg(aerr));
      if (aerr > 0) amanzi_throw(e);
    }
  }
  e.add_data(boost::str(boost::format("%s: unable to read mesh file") %
                        filename).c_str());
  Exceptions::amanzi_throw(e);
  return Teuchos::null;
}

/** 
 * Coellective
 *
 * This creates a mesh by generating a block of hexahedral cells.
 *
 * Hopefully, if any one process has an error, all processes will
 * throw an Mesh::Message exception.
 * 
 * @param x0 origin x-coordinate
 * @param y0 origin y-coordinate
 * @param z0 origin z-coordinate
 * @param x1 maximum x-coordinate
 * @param y1 maximum y-coordinate
 * @param z1 maximum z-coordinate
 * @param nx number of cells in the x-direction
 * @param ny number of cells in the y-direction
 * @param nz number of cells in the z-direction
 * 
 * @return mesh instance
 */
Teuchos::RCP<Mesh> 
MeshFactory::create(double x0, double y0, double z0,
                    double x1, double y1, double z1,
                    int nx, int ny, int nz, 
                    const Teuchos::RCP<const AmanziGeometry::GeometricModel>& gm,
                    const bool request_faces, 
                    const bool request_edges)
{
  Teuchos::RCP<Mesh> result;
  Message e("MeshFactory::create: error: ");
  int ierr, aerr;
  ierr = 0;
  aerr = 0;

  unsigned int dim = 3;

  if (nx <= 0 || ny <= 0 || nz <= 0) {
    ierr += 1;
    e.add_data(boost::str(boost::format("invalid mesh cells requested: %d x %d x %d") %
                          nx % ny % nz).c_str());
  }
  Teuchos::reduceAll(*comm_, Teuchos::REDUCE_SUM, ierr, Teuchos::outArg(aerr));
  if (aerr > 0) amanzi_throw(e);

  if (x1 - x0 <= 0.0 || y1 - y0 <= 0.0 || z1 - z0 <= 0.0) {
    ierr += 1;
    e.add_data(boost::str(boost::format("invalid mesh dimensions requested: %.6g x %.6g x %.6g") %
                          (x1 - x0) % (y1 - y0) % (z1 - z0)).c_str());
  }
  Teuchos::reduceAll(*comm_, Teuchos::REDUCE_SUM, ierr, Teuchos::outArg(aerr));
  if (aerr > 0) amanzi_throw(e);
      
  for (FrameworkPreference::const_iterator i = my_preference_.begin(); 
       i != my_preference_.end(); i++) {
    if (framework_generates(*i, comm_->getSize() > 1, dim)) {
      try {
        result = framework_generate(comm_, *i, 
                                    x0, y0, z0, x1, y1, z1, 
                                    nx, ny, nz,
                                    gm, vo_,
                                    request_faces, request_edges, partitioner_);
        return result;
      } catch (const Message& msg) {
        ierr += 1;
        e.add_data(msg.what());
      } catch (const std::exception& stde) {
        ierr += 1;
        e.add_data("internal error: ");
        e.add_data(stde.what());
      }
      Teuchos::reduceAll(*comm_, Teuchos::REDUCE_SUM, ierr, Teuchos::outArg(aerr));
      if (aerr > 0) amanzi_throw(e);
    }
  }
  e.add_data("unable to generate 3D mesh");
  Exceptions::amanzi_throw(e);
  return Teuchos::null;
}

/** 
 * Coellective
 *
 * This creates a mesh by generating a block of quadrilateral cells.
 *
 * Hopefully, if any one process has an error, all processes will
 * throw an Mesh::Message exception.
 * 
 * @param x0 origin x-coordinate
 * @param y0 origin y-coordinate
 * @param x1 maximum x-coordinate
 * @param y1 maximum y-coordinate
 * @param nx number of cells in the x-direction
 * @param ny number of cells in the y-direction
 * 
 * @return mesh instance
 */
Teuchos::RCP<Mesh> 
MeshFactory::create(double x0, double y0,
                    double x1, double y1,
                    int nx, int ny,
                    const Teuchos::RCP<const AmanziGeometry::GeometricModel>& gm,
                    const bool request_faces, 
                    const bool request_edges)
{
  Teuchos::RCP<Mesh> result;
  Message e("MeshFactory::create: error: ");
  int ierr, aerr;
  ierr = 0;
  aerr = 0;

  unsigned int dim = 2;

  if (nx <= 0 || ny <= 0) {
    ierr += 1;
    e.add_data(boost::str(boost::format("invalid mesh cells requested: %d x %d") %
                          nx % ny).c_str());
  }
  Teuchos::reduceAll(*comm_, Teuchos::REDUCE_SUM, ierr, Teuchos::outArg(aerr));
  if (aerr > 0) amanzi_throw(e);

  if (x1 - x0 <= 0.0 || y1 - y0 <= 0.0) {
    ierr += 1;
    e.add_data(boost::str(boost::format("invalid mesh dimensions requested: %.6g x %.6g") %
                          (x1 - x0) % (y1 - y0)).c_str());
  }
  Teuchos::reduceAll(*comm_, Teuchos::REDUCE_SUM, ierr, Teuchos::outArg(aerr));
  if (aerr > 0) amanzi_throw(e);
      
  for (FrameworkPreference::const_iterator i = my_preference_.begin(); 
       i != my_preference_.end(); i++) {
    if (framework_generates(*i, comm_->getSize() > 1, dim)) {
      try {
        result = framework_generate(comm_, *i, 
                                    x0, y0, x1, y1,
                                    nx, ny,
                                    gm, vo_,
                                    request_faces, request_edges, partitioner_);
        return result;
      } catch (const Message& msg) {
        ierr += 1;
        e.add_data(msg.what());
      } catch (const std::exception& stde) {
        ierr += 1;
        e.add_data("internal error: ");
        e.add_data(stde.what());
      }
      Teuchos::reduceAll(*comm_, Teuchos::REDUCE_SUM, ierr, Teuchos::outArg(aerr));
      if (aerr > 0) amanzi_throw(e);
    }
  }
  e.add_data("unable to generate 2D mesh");
  Exceptions::amanzi_throw(e);
  return Teuchos::null;
}

/** 
 * This creates a mesh by generating a block of
 * quadrilateral/hexahedral cells, but using a parameter list with the
 * limits and cell counts.
 * 
 * @param parameter_list 
 * 
 * @return 
 */
Teuchos::RCP<Mesh> 
MeshFactory::create(Teuchos::ParameterList &parameter_list, 
                    const Teuchos::RCP<const AmanziGeometry::GeometricModel>& gm,
                    const bool request_faces, 
                    const bool request_edges)
{
  Teuchos::RCP<Mesh> result;
  Message e("MeshFactory::create: error: ");
  int ierr, aerr;
  ierr = 0;
  aerr = 0;

  Teuchos::Array<int> ncells = parameter_list.get< Teuchos::Array<int> >("number of cells");
  unsigned int dim = ncells.size();

  for (auto i = my_preference_.begin(); i != my_preference_.end(); i++) {
    if (framework_generates(*i, comm_->getSize() > 1, dim)) {
      try {
        result = framework_generate(comm_, *i, parameter_list, gm, vo_,
                                    request_faces, request_edges);
        return result;
      } catch (const Message& msg) {
        ierr += 1;
        e.add_data(msg.what());
      } catch (const std::exception& stde) {
        ierr += 1;
        e.add_data("internal error: ");
        e.add_data(stde.what());
      }
      Teuchos::reduceAll(*comm_, Teuchos::REDUCE_SUM, ierr, Teuchos::outArg(aerr));
      if (aerr > 0) amanzi_throw(e);
    }
  }
  e.add_data("unable to generate mesh");
  Exceptions::amanzi_throw(e);
  return Teuchos::null;
}

/** 
 * This creates a mesh by extracting subsets of entities from an existing
 * mesh possibly flattening it by removing the last dimension or (in the 
 * future) extruding it when it makes sense
 * 
 * @param inmesh
 * @param setnames
 * @param setkind
 * @param flatten
 * @param extrude
 * 
 * @return 
 */
Teuchos::RCP<Mesh> 
MeshFactory::create(const Mesh *inmesh, 
                    const std::vector<std::string> setnames,
                    const Entity_kind setkind,
                    const bool flatten, const bool extrude,
                    const bool request_faces, 
                    const bool request_edges)
{
  Teuchos::RCP<Mesh> result;
  Message e("MeshFactory::create: error: ");
  int ierr, aerr;
  ierr = 0;
  aerr = 0;

  int dim = inmesh->manifold_dimension();

  for (FrameworkPreference::const_iterator i = my_preference_.begin(); 
       i != my_preference_.end(); i++) {
    if (framework_extracts(*i, comm_->getSize() > 1, dim)) {
      try {
        result = framework_extract(comm_, *i, inmesh, setnames, setkind, 
                                   flatten, extrude,
                                   request_faces, request_edges);
        return result;
      } catch (const Message& msg) {
        ierr += 1;
        e.add_data(msg.what());
      } catch (const std::exception& stde) {
        ierr += 1;
        e.add_data("internal error: ");
        e.add_data(stde.what());
      }
      Teuchos::reduceAll(*comm_, Teuchos::REDUCE_SUM, ierr, Teuchos::outArg(aerr));
      if (aerr > 0) amanzi_throw(e);
    }
  }
  e.add_data("unable to extract mesh");
  Exceptions::amanzi_throw(e);
  return Teuchos::null;
}

} // namespace AmanziMesh
} // namespace Amanzi
