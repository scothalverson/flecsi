/*~--------------------------------------------------------------------------~*
*  @@@@@@@@  @@           @@@@@@   @@@@@@@@ @@
* /@@/////  /@@          @@////@@ @@////// /@@
* /@@       /@@  @@@@@  @@    // /@@       /@@
* /@@@@@@@  /@@ @@///@@/@@       /@@@@@@@@@/@@
* /@@////   /@@/@@@@@@@/@@       ////////@@/@@
* /@@       /@@/@@//// //@@    @@       /@@/@@
* /@@       @@@//@@@@@@ //@@@@@@  @@@@@@@@ /@@
* //       ///  //////   //////  ////////  //
*
* Copyright (c) 2016 Los Alamos National Laboratory, LLC
* All rights reserved
*~--------------------------------------------------------------------------~*/

#ifndef flecsi_execution_legion_init_args_h
#define flecsi_execution_legion_init_args_h

//----------------------------------------------------------------------------//
//! @file
//! @date Initial file creation: May 19, 2017
//----------------------------------------------------------------------------//

#include <vector>

#include "legion.h"

#include "flecsi/data/common/privilege.h"
#include "flecsi/data/data_client_handle.h"

namespace flecsi {
namespace execution {

  //--------------------------------------------------------------------------//
  //! The init_args_t type can be called to walk task args before the
  //! task launcher is created. This allows us to gather region requirements
  //! and to set state on the associated data handles \em before Legion gets
  //! the task arguments tuple.
  //!
  //! @ingroup execution
  //--------------------------------------------------------------------------//

  struct init_args_t : public utils::tuple_walker__<init_args_t>
  {

    //------------------------------------------------------------------------//
    //! Construct an init_args_t instance.
    //!
    //! @param runtime The Legion task runtime.
    //! @param context The Legion task runtime context.
    //------------------------------------------------------------------------//

    init_args_t(
      Legion::Runtime* runtime,
      Legion::Context & context
    )
    :
      runtime(runtime),
      context(context)
    {
    } // init_args

    //------------------------------------------------------------------------//
    //! Convert the template privileges to proper Legion privileges.
    //!
    //! @param mode privilege
    //------------------------------------------------------------------------//

    static Legion::PrivilegeMode
    privilege_mode(
      size_t mode
    ){
      switch(mode){
        case size_t(dno):
          return NO_ACCESS;
        case size_t(dro):
          return READ_ONLY;
        case size_t(dwd):
          return WRITE_DISCARD;
        case size_t(drw):
          return READ_WRITE;
        default:
          assert(false);
      }
    }

    template<
      typename T,
      size_t EXCLUSIVE_PERMISSIONS,
      size_t SHARED_PERMISSIONS,
      size_t GHOST_PERMISSIONS
    >
    void
    handle(
      data_handle__<
        T,
        EXCLUSIVE_PERMISSIONS,
        SHARED_PERMISSIONS,
        GHOST_PERMISSIONS
      > & h
    )
    {
      Legion::RegionRequirement ex_rr(h.exclusive_lr,
        privilege_mode(EXCLUSIVE_PERMISSIONS), EXCLUSIVE, h.color_region);
      ex_rr.add_field(h.fid);
      region_reqs.push_back(ex_rr);

      Legion::RegionRequirement sh_rr(h.shared_lr,
        privilege_mode(SHARED_PERMISSIONS), EXCLUSIVE, h.color_region);
      sh_rr.add_field(h.fid);
      region_reqs.push_back(sh_rr);

      Legion::RegionRequirement gh_rr(h.ghost_lr,
        privilege_mode(GHOST_PERMISSIONS), EXCLUSIVE, h.color_region);
      gh_rr.add_field(h.fid);
      region_reqs.push_back(gh_rr);
    } // handle

    template<
      typename T
    >
    void
    handle(
      data_client_handle__<T> & h
    )
    {
      auto& context_ = context_t::instance();

      auto& ism = context_.index_space_data_map();

      auto adjacency_index_fid = 
        LegionRuntime::HighLevel::FieldID(internal_field::adjacency_index);

      for(size_t i = 0; i < h.num_adjacencies; ++i){
        size_t adj_index_space = h.adj_index_spaces[i];
        size_t from_index_space = h.from_index_spaces[i];
        size_t to_index_space = h.to_index_spaces[i];

        Legion::RegionRequirement from_rr(ism[from_index_space].color_region,
          READ_ONLY, EXCLUSIVE, ism[from_index_space].color_region);

        from_rr.add_field(context_.entity_data_fid(from_index_space));

        Legion::FieldID offset_fid = 
          context_.adjacency_fid(from_index_space, to_index_space);

        from_rr.add_field(offset_fid);

        region_reqs.push_back(from_rr);

        Legion::RegionRequirement to_rr(ism[to_index_space].color_region,
          READ_ONLY, EXCLUSIVE, ism[to_index_space].color_region);

        to_rr.add_field(context_.entity_data_fid(to_index_space));

        region_reqs.push_back(to_rr);

        Legion::RegionRequirement adj_rr(ism[adj_index_space].color_region,
          READ_ONLY, EXCLUSIVE, ism[adj_index_space].color_region);

        adj_rr.add_field(adjacency_index_fid);

        region_reqs.push_back(adj_rr);

      }
    } // handle

    //-----------------------------------------------------------------------//
    // If this is not a data handle, then simply skip it.
    //-----------------------------------------------------------------------//

    template<
      typename T
    >
    static
    typename std::enable_if_t<!std::is_base_of<data_handle_base_t, T>::value>
    handle(
      T &
    )
    {
    } // handle

    Legion::Runtime * runtime;
    Legion::Context & context;
    std::vector<Legion::RegionRequirement> region_reqs;

  }; // struct init_args_t

} // namespace execution 
} // namespace flecsi

#endif // flecsi_execution_legion_init_args_h
