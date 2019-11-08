/*
    @@@@@@@@  @@           @@@@@@   @@@@@@@@ @@
   /@@/////  /@@          @@////@@ @@////// /@@
   /@@       /@@  @@@@@  @@    // /@@       /@@
   /@@@@@@@  /@@ @@///@@/@@       /@@@@@@@@@/@@
   /@@////   /@@/@@@@@@@/@@       ////////@@/@@
   /@@       /@@/@@//// //@@    @@       /@@/@@
   /@@       @@@//@@@@@@ //@@@@@@  @@@@@@@@ /@@
   //       ///  //////   //////  ////////  //

   Copyright (c) 2016, Triad National Security, LLC
   All rights reserved.
                                                                              */

#include <flecsi-config.h>

#if !defined(__FLECSI_PRIVATE__)
#define __FLECSI_PRIVATE__
#endif

#include <flecsi/data.hh>
#include <flecsi/execution/launch.hh>
#include <flecsi/execution/legion/task_wrapper.hh>
#include <flecsi/runtime/legion/context.hh>
#include <flecsi/runtime/legion/mapper.hh>
#include <flecsi/runtime/legion/tasks.hh>
#include <flecsi/runtime/program_options.hh>
#include <flecsi/runtime/types.hh>
#include <flecsi/utils/const_string.hh>

namespace flecsi::runtime {

using namespace boost::program_options;
using execution::legion::task_id;

//----------------------------------------------------------------------------//
// Implementation of context_t::initialize.
//----------------------------------------------------------------------------//

int
context_t::initialize(int argc, char ** argv, bool dependent) {

  if(dependent) {
    int version, subversion;
    MPI_Get_version(&version, &subversion);

#if defined(GASNET_CONDUIT_MPI)
    if(version == 3 && subversion > 0) {
      int provided;
      MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &provided);

      if(provided < MPI_THREAD_MULTIPLE) {
        std::cerr << "Your implementation of MPI does not support "
                     "MPI_THREAD_MULTIPLE which is required for use of the "
                     "GASNet MPI conduit with the Legion-MPI Interop!"
                  << std::endl;
        std::abort();
      } // if
    }
    else {
      // Initialize the MPI runtime
      MPI_Init(&argc, &argv);
    } // if
#else
    MPI_Init(&argc, &argv);
#endif
  } // if

  int rank, size;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &size);

  context::process_ = rank;
  context::processes_ = size;

  auto status = context::initialize_generic(argc, argv, dependent);

  if(status != success && dependent) {
    MPI_Finalize();
  } // if

  return status;
} // initialize

//----------------------------------------------------------------------------//
// Implementation of context_t::finalize.
//----------------------------------------------------------------------------//

int
context_t::finalize() {

  auto status = context::finalize_generic();

#ifndef GASNET_CONDUIT_MPI
  if(status == success && context::initialize_dependent_) {
    MPI_Finalize();
  } // if
#endif

  return status;
} // finalize

//----------------------------------------------------------------------------//
// Implementation of context_t::start.
//----------------------------------------------------------------------------//

int
context_t::start() {
  using namespace Legion;

  /*
    Setup Legion top-level task.
   */

  Runtime::set_top_level_task_id(FLECSI_TOP_LEVEL_TASK_ID);

  {
    Legion::TaskVariantRegistrar registrar(
      FLECSI_TOP_LEVEL_TASK_ID, "runtime_driver");
    registrar.add_constraint(ProcessorConstraint(Processor::LOC_PROC));
    //    registrar.set_inner();
    registrar.set_replicable();
    Runtime::preregister_task_variant<top_level_task>(
      registrar, "runtime_driver");
  } // scope

  /*
    Setup internal launch domains.
   */

  set_launch_domain_size(single, 1);
  set_launch_domain_size(index, 0);

  /*
    Register tasks.
   */

  {
    flog_tag_guard(context);
    flog_devel(info) << "Invoking task registration callbacks" << std::endl;
  }

  for(auto && p : task_registry_)
    p();

  /*
    Arg 0: MPI has initial control (true).
    Arg 1: Number of MPI participants (1).
    Arg 2: Number of Legion participants (1).
   */

  handshake_ = Legion::Runtime::create_handshake(true, 1, 1);

  /*
    Register custom mapper.
   */

  Runtime::add_registration_callback(mapper_registration);

  /*
    Configure interoperability layer.
   */

  Legion::Runtime::configure_MPI_interoperability(context::process_);

  /*
    Register reduction operations.
   */

  for(auto & ro : context::reduction_registry()) {
    ro.second();
  } // for

  /*
    Handle command-line arguments.
   */

  std::vector<char *> largv;
  largv.push_back(argv_[0]);
  context::threads_per_process_ = 1;

  for(auto opt = unrecognized_options_.begin();
      opt != unrecognized_options_.end();
      ++opt) {

    // FIXME: This case is a temporary fix until we decide
    // what to do about Issue #8: Default Launch Domain Size.
    if(opt->find("-ll:cpu") != std::string::npos) {
      largv.push_back(opt->data());
      largv.push_back((++opt)->data());
      std::stringstream sstream(largv.back());
      sstream >> context::threads_per_process_;
    }
    else {
      largv.push_back(opt->data());
    } // if
  } // for

  context::threads_ = context::processes_ * context::threads_per_process_;

  /*
    Start Legion runtime.
   */

  {
    flog_tag_guard(context);

    std::stringstream stream;

    stream << "Starting Legion runtime" << std::endl;
    stream << "\targc: " << largv.size() << std::endl;
    stream << "\targv: ";

    for(auto opt : largv) {
      stream << opt << " ";
    } // for

    stream << std::endl;

    flog_devel(info) << stream.str();
  } // scope

  Runtime::start(largv.size(), largv.data(), true);

  do {
    handoff_to_legion();
    wait_on_legion();
  } while(invoke_mpi_task());

  // Make sure that the flusher thread executes at least one cycle.
  __flog_internal_wait_on_flusher();

  Legion::Runtime::wait_for_shutdown();

  return context::exit_status();
} // context_t::start

//----------------------------------------------------------------------------//
// Implementation of context_t::handoff_to_mpi.
//----------------------------------------------------------------------------//

void
context_t::handoff_to_mpi(Legion::Context & ctx, Legion::Runtime * runtime) {
  Legion::ArgumentMap arg_map;
  Legion::IndexLauncher handoff_to_mpi_launcher(task_id<handoff_to_mpi_task>,
    Legion::Domain::from_rect<1>(context_t::instance().all_processes()),
    Legion::TaskArgument(NULL, 0),
    arg_map);

  handoff_to_mpi_launcher.tag = FLECSI_MAPPER_FORCE_RANK_MATCH;
  auto fm = runtime->execute_index_space(ctx, handoff_to_mpi_launcher);

  fm.wait_all_results(true);
} // context_t::handoff_to_mpi

//----------------------------------------------------------------------------//
// Implementation of context_t::wait_on_mpi.
//----------------------------------------------------------------------------//

Legion::FutureMap
context_t::wait_on_mpi(Legion::Context & ctx, Legion::Runtime * runtime) {
  Legion::ArgumentMap arg_map;
  Legion::IndexLauncher wait_on_mpi_launcher(task_id<wait_on_mpi_task>,
    Legion::Domain::from_rect<1>(context_t::instance().all_processes()),
    Legion::TaskArgument(NULL, 0),
    arg_map);

  wait_on_mpi_launcher.tag = FLECSI_MAPPER_FORCE_RANK_MATCH;
  auto fm = runtime->execute_index_space(ctx, wait_on_mpi_launcher);

  fm.wait_all_results(true);

  return fm;
} // context_t::wait_on_mpi

//----------------------------------------------------------------------------//
// Implementation of context_t::connect_with_mpi.
//----------------------------------------------------------------------------//

void
context_t::connect_with_mpi(Legion::Context & ctx, Legion::Runtime * runtime) {
  int size;
  MPI_Comm_size(MPI_COMM_WORLD, &size);

  LegionRuntime::Arrays::Rect<1> launch_bounds(
    LegionRuntime::Arrays::Point<1>(0),
    LegionRuntime::Arrays::Point<1>(size - 1));

  context_t::instance().set_all_processes(launch_bounds);

  const std::map<int, Legion::AddressSpace> & forward_mapping =
    runtime->find_forward_MPI_mapping();

#if 0
  for(std::map<int, Legion::AddressSpace>::const_iterator it =
        forward_mapping.begin();
      it != forward_mapping.end(); it++)
    printf(
      "MPI Rank %d maps to Legion Address Space %d\n", it->first, it->second);
#endif
} // context_t::connect_with_mpi

//----------------------------------------------------------------------------//
// Implementation of initialize_global_topology.
//----------------------------------------------------------------------------//

void
context_t::initialize_global_topology() {
  using namespace Legion;

  auto legion_runtime_ = Legion::Runtime::get_runtime();
  auto legion_context_ = Legion::Runtime::get_context();

  LegionRuntime::Arrays::Rect<1> bounds(
    LegionRuntime::Arrays::Point<1>(0), LegionRuntime::Arrays::Point<1>(1));

  Domain dom(Domain::from_rect<1>(bounds));

  auto & global_topology_instance_ = global_topology.allocate({});

  global_topology_instance_.index_space =
    legion_runtime_->create_index_space(legion_context_, dom);

  global_topology_instance_.field_space =
    legion_runtime_->create_field_space(legion_context_);

  FieldAllocator allocator = legion_runtime_->create_field_allocator(
    legion_context_, global_topology_instance_.field_space);

  /*
    Note: This call to get_field_info_store uses the non-const version
    so that this call works if no fields have been registered. In other parts
    of the code that occur after initialization, the const version of this call
    should be used.
   */

  auto & field_info_store = context_t::instance().get_field_info_store(
    topology::id<topology::global_t>(), flecsi::data::storage_label_t::dense);

  for(auto const & fi : field_info_store) {
    allocator.allocate_field(fi.type_size, fi.fid);
  } // for

  global_topology_instance_.logical_region =
    legion_runtime_->create_logical_region(legion_context_,
      global_topology_instance_.index_space,
      global_topology_instance_.field_space);
} // context_t::initialize_global_topology

//----------------------------------------------------------------------------//
// Implementation of finalize_global_topology.
//----------------------------------------------------------------------------//

void
context_t::finalize_global_topology() {
  global_topology.deallocate();
} // context_t::finalize_global_topology

//----------------------------------------------------------------------------//
// Index coloring and topology method implementations.
//----------------------------------------------------------------------------//

void
context_t::initialize_process_coloring() {
  process_coloring.allocate(processes_);
} // context_t::initialize_process_coloring

void
context_t::initialize_process_topology() {
  process_topology.allocate(process_coloring.get());
} // context_t::initialize_process_topology

void
context_t::finalize_process_topology() {
  process_topology.deallocate();
} // context_t::finalize_process_topology

} // namespace flecsi::runtime
