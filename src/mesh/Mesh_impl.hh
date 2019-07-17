/* -*-  mode: c++; c-default-style: "google"; indent-tabs-mode: nil -*- */
//
// Base mesh class for Amanzi
//
// Inline implementations
//

#include "Kokkos_Core.hpp"

// Downward Adjacencies
//---------------------
void
KOKKOS_INLINE_FUNCTION Mesh::cell_get_faces(const Entity_ID cellid, Kokkos::View<Entity_ID*>& faceids,
                     const bool ordered) const
{
  cell_get_faces_and_dirs(cellid, faceids, NULL, ordered);
}


//
// Column information
//-----------------------
inline
int
Mesh::num_columns(bool ghosted) const
{
  if (!columns_built_) {
    Errors::Message mesg("num_columns called before calling build_columns");
    Exceptions::amanzi_throw(mesg);
  }
  return ghosted ? column_cells_.row_map.extent(0)-1 : num_owned_cols_; // number of vector of vectors
}


inline
Kokkos::View<Entity_ID*>
Mesh::cells_of_column(const int columnID) const
{
  if (!columns_built_) {
    Errors::Message mesg("cells_of_column called before calling build_columns");
    Exceptions::amanzi_throw(mesg);
  }
  return Kokkos::subview(column_cells_.entries,
    std::make_pair(column_cells_.row_map(columnID),column_cells_.row_map(columnID+1)));
}


inline
Kokkos::View<Entity_ID*>
Mesh::faces_of_column(const int columnID) const
{
  if (!columns_built_) {
    Errors::Message mesg("faces_of_columns called before calling build_columns");
    Exceptions::amanzi_throw(mesg);
  }
  return Kokkos::subview(column_faces_.entries,
    std::make_pair(column_faces_.row_map(columnID),column_faces_.row_map(columnID+1)));
}


inline
int
Mesh::column_ID(const Entity_ID cellid) const
{
  if (!columns_built_) {
    Errors::Message mesg("column_ID called before calling build_columns");
    Exceptions::amanzi_throw(mesg);
  }
  return columnID_[cellid];
}


inline
Entity_ID
Mesh::cell_get_cell_above(const Entity_ID cellid) const
{
  if (!columns_built_) {
    Errors::Message mesg("cell_get_cell_above called before calling build_columns");
    Exceptions::amanzi_throw(mesg);
  }
  return cell_cellabove_[cellid];
}


inline
Entity_ID
Mesh::cell_get_cell_below(const Entity_ID cellid) const
{
  if (!columns_built_) {
    Errors::Message mesg("cell_get_cell_below called before calling build_columns");
    Exceptions::amanzi_throw(mesg);
  }
  return cell_cellbelow_[cellid];
}


inline
Entity_ID
Mesh::node_get_node_above(const Entity_ID nodeid) const
{
  if (!columns_built_) {
    Errors::Message mesg("node_get_node_above called before calling build_columns");
    Exceptions::amanzi_throw(mesg);
  }
  return node_nodeabove_[nodeid];
}


//
// Column information
//-----------------------
inline
void
Mesh::get_set_entities(const std::string setname,
                       const Entity_kind kind,
                       const Parallel_type ptype,
                       Kokkos::View<Entity_ID*> &entids) const
{
  Kokkos::View<double*> vofs;
  get_set_entities_and_vofs(setname, kind, ptype, entids, &vofs);
}
