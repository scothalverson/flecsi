/*~--------------------------------------------------------------------------~*
 * Copyright (c) 2015 Los Alamos National Security, LLC
 * All rights reserved.
 *~--------------------------------------------------------------------------~*/

#ifndef flecsi_task_h
#define flecsi_task_h

#include "flecsi/execution/default_execution_policy.h"

#include "flecsi/utils/const_string.h"

/*!
 * \file task.h
 * \authors bergen
 * \date Initial file creation: Jul 26, 2016
 */

namespace flecsi {

/*!
  \struct task__ task.h
  \brief task__ provides...
 */
template<typename execution_policy_t = default_execution_policy_t>
struct task__
{

#if 0
  template<typename T>
  static decltype(auto) register_task(const const_string_t & key, T && task)
  {
    return execution_policy_t::register_task(key, std::forward<T>(task));
  } // register_task
#endif

  static decltype(auto) register_task(const const_string_t & key)
  {
    return execution_policy_t::register_task(key);
  } // register_task

  template<typename ... Args>
  static decltype(auto) execute_task(const const_string_t & key,
    Args && ... args)
  {
    return execution_policy_t::execute_task(key, std::forward<Args>(args) ...);
  } // execute

}; // class task

} // namespace flecsi

#include "flecsi/execution/legion/legion_execution_policy.h"

namespace flecsi {
using task_t = task__<legion_execution_policy_t>;
} // namespace flecsi

#endif // flecsi_task_h

/*~-------------------------------------------------------------------------~-*
 * Formatting options for vim.
 * vim: set tabstop=2 shiftwidth=2 expandtab :
 *~-------------------------------------------------------------------------~-*/
