/*
    @@@@@@@@  @@           @@@@@@   @@@@@@@@ @@
   /@@/////  /@@          @@////@@ @@////// /@@
   /@@       /@@  @@@@@  @@    // /@@       /@@
   /@@@@@@@  /@@ @@///@@/@@       /@@@@@@@@@/@@
   /@@////   /@@/@@@@@@@/@@       ////////@@/@@
   /@@       /@@/@@//// //@@    @@       /@@/@@
   /@@       @@@//@@@@@@ //@@@@@@  @@@@@@@@ /@@
   //       ///  //////   //////  ////////  //

   Copyright (c) 2016, Los Alamos National Security, LLC
   All rights reserved.
                                                                              */
#pragma once

/*! @file */

#if !defined(__FLECSI_PRIVATE__)
#error Do not inlcude this file directly!
#else
#include <flecsi/execution/context.h>
#include <flecsi/execution/legion/reduction_wrapper.h>
#include <flecsi/execution/legion/task_wrapper.h>
#include <flecsi/utils/flog.h>
#endif

#include <functional>
#include <memory>
#include <type_traits>

#include <flecsi-config.h>

#if !defined(FLECSI_ENABLE_LEGION)
#error FLECSI_ENABLE_LEGION not defined! This file depends on Legion!
#endif

#include <legion.h>

namespace flecsi {
namespace execution {

//----------------------------------------------------------------------------//
// Execution policy.
//----------------------------------------------------------------------------//

/*!
  The legion_execution_policy_t is the backend runtime execution policy
  for Legion.

  @ingroup legion-execution
 */

struct legion_execution_policy_t {

  //--------------------------------------------------------------------------//
  // Task interface.
  //--------------------------------------------------------------------------//

  /*!
    This method allows the user to register a pure Legion task with
    the runtime. A task id will automatically be generated, and can be
    accessed via legion_context_policy_t::task_id using a valid
    task hash.

    @tparam TASK     A hash key identifying the task.
    @tparam RETURN   The return type of the pure Legion task.
    @tparam DELEGATE The function pointer template type of the task.

    @param name The string name for the task. This can be set to any
                valid std::string value.
   */

  template<size_t TASK,
    typename RETURN,
    RETURN (*DELEGATE)(const Legion::Task *,
      const std::vector<Legion::PhysicalRegion> &,
      Legion::Context,
      Legion::Runtime *)>
  static bool register_legion_task(processor_type_t processor,
    launch_t launch,
    std::string name) {
    flog(internal) << "Registering legion task" << std::endl
                   << "\tname: " << name << std::endl
                   << "\thash: " << TASK << std::endl;

    using wrapper_t = legion::pure_task_wrapper_u<RETURN, DELEGATE>;

    const bool success = context_t::instance().register_task(
      TASK, processor, launch, name, wrapper_t::registration_callback);

    flog_assert(success, "callback registration failed for " << name);

    return true;
  } // register_legion_task

  /*
    Documentation for this interface is in the top-level context type.
   */

  template<size_t TASK,
    typename RETURN,
    typename ARG_TUPLE,
    RETURN (*DELEGATE)(ARG_TUPLE)>
  static bool
  register_task(processor_type_t processor, launch_t launch, std::string name) {

    using wrapper_t = task_wrapper_u<TASK, RETURN, ARG_TUPLE, DELEGATE>;

    const bool success = context_t::instance().register_task(
      TASK, processor, launch, name, wrapper_t::registration_callback);

    flog_assert(success, "callback registration failed for " << name);

    return true;
  } // register_task

  //------------------------------------------------------------------------//
  // Reduction interface.
  //------------------------------------------------------------------------//

  template<size_t HASH, typename TYPE>
  using reduction_wrapper_u = legion::reduction_wrapper_u<HASH, TYPE>;

}; // struct legion_execution_policy_t

} // namespace execution
} // namespace flecsi
