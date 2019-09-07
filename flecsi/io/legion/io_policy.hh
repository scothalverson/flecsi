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
#pragma once

/*!  @file */

#include <flecsi-config.h>

#if !defined(FLECSI_ENABLE_LEGION)
#error FLECSI_ENABLE_LEGION not defined! This file depends on Legion!
#endif

#include "flecsi/runtime/backend.hh"

#include <hdf5.h>
#include <legion.h>

flog_register_tag(io);

namespace flecsi {
namespace io {
  
#define SERIALIZATION_BUFFER_SIZE 4096
  
void
checkpoint_with_attach_task(const Legion::Task * task,
  const std::vector<Legion::PhysicalRegion> & regions,
  Legion::Context ctx,
  Legion::Runtime * runtime);
  
void
checkpoint_without_attach_task(const Legion::Task * task,
  const std::vector<Legion::PhysicalRegion> & regions,
  Legion::Context ctx,
  Legion::Runtime * runtime);
  
void
recover_with_attach_task(const Legion::Task * task,
  const std::vector<Legion::PhysicalRegion> & regions,
  Legion::Context ctx,
  Legion::Runtime * runtime);
  
void
recover_without_attach_task(const Legion::Task * task,
  const std::vector<Legion::PhysicalRegion> & regions,
  Legion::Context ctx,
  Legion::Runtime * runtime);

/*----------------------------------------------------------------------------*
  Argumemt of Legion checkpoint and recover tasks, not called by users.
 *----------------------------------------------------------------------------*/
struct checkpoint_task_args_s {
  size_t field_map_size;
  char field_map_serial[SERIALIZATION_BUFFER_SIZE];
  char file_name[32];
};

/*----------------------------------------------------------------------------*
  HDF5 descriptor of one logical region, not called by users.
 *----------------------------------------------------------------------------*/
struct legion_hdf5_region_t {
  legion_hdf5_region_t(Legion::LogicalRegion lr,
    Legion::LogicalPartition lp,
    std::string lr_name,
    std::map<Legion::FieldID, std::string> & field_string_map)
    : logical_region(lr), logical_partition(lp), logical_region_name(lr_name),
        field_string_map(field_string_map) {    
    Legion::Runtime * runtime = Legion::Runtime::get_runtime();
    Legion::Context ctx = Legion::Runtime::get_context();
    if(lr.get_dim() == 1) {
      Legion::Domain domain = runtime->get_index_space_domain(ctx, lr.get_index_space());
      dim_size[0] = domain.get_volume();
      {
        flog_tag_guard(io);
        flog_devel(info) << "ID logical region size " << dim_size[0]
                         << std::endl;
      }
    }
    else {
      Legion::Domain domain = runtime->get_index_space_domain(ctx, lr.get_index_space());
      dim_size[0] = domain.get_volume();
      {
        flog_tag_guard(io);
        flog_devel(info) << "ID logical region size " << dim_size[0]
                         << std::endl;
      }
    }
  }

  legion_hdf5_region_t(Legion::LogicalRegion lr,
    Legion::LogicalPartition lp,
    std::string lr_name)
    : logical_region(lr), logical_partition(lp), logical_region_name(lr_name) {
  }

  Legion::LogicalRegion logical_region;
  Legion::LogicalPartition logical_partition;
  std::string logical_region_name;
  std::map<Legion::FieldID, std::string> field_string_map;
  size_t dim_size[3];
};

/*----------------------------------------------------------------------------*
  HDF5 file, not called by users.
 *----------------------------------------------------------------------------*/
struct legion_hdf5_t {
  
//----------------------------------------------------------------------------//
// Implementation of legion_hdf5_t::legion_hdf5_t.
//----------------------------------------------------------------------------//
  legion_hdf5_t(std::string file_name, int num_files)
  : hdf5_file_id(-1), file_name(file_name), num_files(num_files) {
    hdf5_region_vector.clear();
    hdf5_group_map.clear();
    {
      flog_tag_guard(io);
      flog_devel(info) << "Init HDF5 file " << file_name
                       << " num_files " << num_files
                       << std::endl;
    }
  }
    
//----------------------------------------------------------------------------//
// Implementation of legion_hdf5_t::legion_hdf5_t.
//----------------------------------------------------------------------------//  
  legion_hdf5_t(const char * file_name, int num_files)
  : legion_hdf5_t(std::string(file_name), num_files) {
  }

//----------------------------------------------------------------------------//
// Implementation of legion_hdf5_t::create_hdf5_file.
//----------------------------------------------------------------------------//    
  bool create_hdf5_file(int file_idx) {
    assert(hdf5_file_id == -1);
    std::string fname = file_name + std::to_string(file_idx);
    hdf5_file_id =
      H5Fcreate(fname.c_str(), H5F_ACC_TRUNC, H5P_DEFAULT, H5P_DEFAULT);
    if(hdf5_file_id < 0) {
      flog(error) << "H5Fcreate failed: " << hdf5_file_id << std::endl;
      return false;
    }
    {
      flog_tag_guard(io);
      flog_devel(info) << "Create HDF5 file " << fname
                       << " file_id " << hdf5_file_id
                       << std::endl;
    }
    return true;
  }

//----------------------------------------------------------------------------//
// Implementation of legion_hdf5_t:open_hdf5_file.
//----------------------------------------------------------------------------//    
  bool open_hdf5_file(int file_idx) {
    assert(hdf5_file_id == -1);
    std::string fname = file_name + std::to_string(file_idx);
    hdf5_file_id = H5Fopen(fname.c_str(), H5F_ACC_RDWR, H5P_DEFAULT);
    if(hdf5_file_id < 0) {
      flog(error) << "H5Fopen failed: " << hdf5_file_id << std::endl;
      return false;
    }
    {
      flog_tag_guard(io);
      flog_devel(info) << "Open HDF5 file " << fname
                       << " file_id " << hdf5_file_id
                       << std::endl;
    }
    return true;
  }

//----------------------------------------------------------------------------//
// Implementation of legion_hdf5_t:close_hdf5_file.
//----------------------------------------------------------------------------//    
  bool close_hdf5_file() {
    assert(hdf5_file_id >= 0);
    H5Fflush(hdf5_file_id, H5F_SCOPE_LOCAL);
    H5Fclose(hdf5_file_id);
    {
      flog_tag_guard(io);
      flog_devel(info) << "Close HDF5 file_id " << hdf5_file_id
                       << std::endl;
    }
    hdf5_file_id = -1;
    return true;
  }

//----------------------------------------------------------------------------//
// Implementation of legion_hdf5_t::write_string_to_hdf5_file.
//----------------------------------------------------------------------------//    
  bool write_string_to_hdf5_file(const char * group_name,
    const char * dataset_name,
    const char * str,
    size_t size) {

    assert(hdf5_file_id >= 0);

    herr_t status;
    // TODO:FIXME
    // status = H5Eset_auto(NULL, NULL);
    // status = H5Gget_objinfo (hdf5_file_id, group_name, 0, NULL);

    hid_t group_id;
    std::map<std::string, hid_t>::iterator it;
    it = hdf5_group_map.find(std::string(group_name));
    if(it != hdf5_group_map.end()) {
      group_id = H5Gopen2(hdf5_file_id, group_name, H5P_DEFAULT);
    }
    else {
      group_id = H5Gcreate2(
        hdf5_file_id, group_name, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
      hdf5_group_map[std::string(group_name)] = group_id;
    }
    if(group_id < 0) {
      flog(error) << "H5Gcreate2 failed: " << group_id << std::endl;
      H5Fclose(hdf5_file_id);
      return false;
    }

    hid_t filetype = H5Tcopy(H5T_C_S1);
    status = H5Tset_size(filetype, H5T_VARIABLE);
    hid_t memtype = H5Tcopy(H5T_C_S1);
    status = H5Tset_size(memtype, H5T_VARIABLE);

    hsize_t dims[1];
    dims[0] = 1;
    hid_t dataspace_id = H5Screate_simple(1, dims, NULL);

    const char * data[1];
    data[0] = str;
    hid_t dset = H5Dcreate2(group_id,
      dataset_name,
      filetype,
      dataspace_id,
      H5P_DEFAULT,
      H5P_DEFAULT,
      H5P_DEFAULT);
    status = H5Dwrite(dset, memtype, H5S_ALL, H5S_ALL, H5P_DEFAULT, data);

    H5Fflush(hdf5_file_id, H5F_SCOPE_LOCAL);
    status = H5Dclose(dset);
    status = H5Sclose(dataspace_id);
    status = H5Tclose(filetype);
    status = H5Tclose(memtype);
    status = H5Gclose(group_id);
    return true;
  }

//----------------------------------------------------------------------------//
// Implementation of legion_hdf5_t::read_string_from_hdf5_file.
//----------------------------------------------------------------------------//      
  bool read_string_from_hdf5_file(const char * group_name,
    const char * dataset_name,
    std::string & str) {
    
    assert(hdf5_file_id >= 0);

    herr_t status;
    // TODO:FIXME
    // status = H5Eset_auto(NULL, NULL);
    // status = H5Gget_objinfo (hdf5_file_id, group_name, 0, NULL);

    hid_t group_id;
    group_id = H5Gopen2(hdf5_file_id, group_name, H5P_DEFAULT);

    if(group_id < 0) {
      flog(error) << "H5Gcreate2 failed: " << group_id << std::endl;
      H5Fclose(hdf5_file_id);
      return false;
    }

    hid_t dset = H5Dopen2(group_id, dataset_name, H5P_DEFAULT);

    hid_t filetype = H5Dget_type(dset);
    hid_t memtype = H5Tcopy(H5T_C_S1);
    status = H5Tset_size(memtype, H5T_VARIABLE);

    char * data[1];
    status = H5Dread(dset, memtype, H5S_ALL, H5S_ALL, H5P_DEFAULT, data);

    str = str + std::string(data[0]);
    H5Fflush(hdf5_file_id, H5F_SCOPE_LOCAL);

    hid_t space = H5Dget_space(dset);
    status = H5Dvlen_reclaim(memtype, space, H5P_DEFAULT, data);
    status = H5Dclose(dset);
    status = H5Tclose(filetype);
    status = H5Tclose(memtype);
    status = H5Gclose(group_id);
    return true;    
  }

//----------------------------------------------------------------------------//
// Implementation of legion_hdf5_t::add_logical_region.
//----------------------------------------------------------------------------//    
  void add_logical_region(Legion::LogicalRegion lr,
    Legion::LogicalPartition lp,
    std::string lr_name,
    std::map<Legion::FieldID, std::string> field_string_map) {
    legion_hdf5_region_t h5_lr(lr, lp, lr_name, field_string_map);
    hdf5_region_vector.push_back(h5_lr);    
  }

//----------------------------------------------------------------------------//
// Implementation of legion_hdf5_t::add_hdf5_region.
//----------------------------------------------------------------------------//    
  void add_hdf5_region(const legion_hdf5_region_t & hdf5_region) {
    hdf5_region_vector.push_back(hdf5_region);
  }

//----------------------------------------------------------------------------//
// Implementation of legion_hdf5_t::create_datasets_for_regions.
//----------------------------------------------------------------------------//    
  bool create_datasets_for_regions(int file_idx) {
    assert(hdf5_region_vector.size() > 0);
    assert(hdf5_file_id >= 0);

    Legion::Runtime * runtime = Legion::Runtime::get_runtime();
    Legion::Context ctx = Legion::Runtime::get_context();
    
    {
      flog_tag_guard(io);
      flog_devel(info) << "Create HDF5 datasets file_id " << hdf5_file_id 
                       << " regions size " << hdf5_region_vector.size()
                       << std::endl;
    }

    for(std::vector<legion_hdf5_region_t>::iterator lr_it =
          hdf5_region_vector.begin();
        lr_it != hdf5_region_vector.end();
        ++lr_it) {
      hid_t dataspace_id = -1;
      if((*lr_it).logical_region.get_index_space().get_dim() == 1) {
        Legion::LogicalRegion sub_lr = runtime->get_logical_subregion_by_color(
          ctx, (*lr_it).logical_partition, file_idx);
        Legion::Domain domain =
          runtime->get_index_space_domain(ctx, sub_lr.get_index_space());
        hsize_t dims[1];
        dims[0] = domain.get_volume();
        dataspace_id = H5Screate_simple(1, dims, NULL);
      }
      else {
        Legion::LogicalRegion sub_lr = runtime->get_logical_subregion_by_color(
          ctx, (*lr_it).logical_partition, file_idx);
        Legion::Domain domain =
          runtime->get_index_space_domain(ctx, sub_lr.get_index_space());
        hsize_t dims[1];
        dims[0] = domain.get_volume();
        dataspace_id = H5Screate_simple(1, dims, NULL);
      }
      if(dataspace_id < 0) {
        flog(error) << "H5Screate_simple failed: " << dataspace_id << std::endl;
        H5Fclose(hdf5_file_id);
        return false;
      }
  #if 0
      hid_t group_id = H5Gcreate2(file_id, (*lr_it).logical_region_name.c_str(), H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
      if (group_id < 0) {
        printf("H5Gcreate2 failed: %lld\n", (long long)group_id);
        H5Sclose(dataspace_id);
        H5Fclose(hdf5_file_id);
        return false;
      }
  #endif
      for(std::map<Legion::FieldID, std::string>::iterator it =
            (*lr_it).field_string_map.begin();
          it != (*lr_it).field_string_map.end();
          ++it) {
        const char * dataset_name = (it->second).c_str();
        hid_t dataset = H5Dcreate2(hdf5_file_id,
          dataset_name,
          H5T_IEEE_F64LE,
          dataspace_id,
          H5P_DEFAULT,
          H5P_DEFAULT,
          H5P_DEFAULT);
        if(dataset < 0) {
          flog(error) << "H5Dcreate2 failed: " << dataset << std::endl;
          //    H5Gclose(group_id);
          H5Sclose(dataspace_id);
          H5Fclose(hdf5_file_id);
          return false;
        }
        H5Dclose(dataset);
      }
      //   H5Gclose(group_id);
      H5Sclose(dataspace_id);
    }
    H5Fflush(hdf5_file_id, H5F_SCOPE_LOCAL);
    return true;
  }

  hid_t hdf5_file_id;
  std::string file_name;
  int num_files;
  std::vector<legion_hdf5_region_t> hdf5_region_vector;
  std::map<std::string, hid_t> hdf5_group_map;
};

/*----------------------------------------------------------------------------*
  Legion HDF5 checkpoint interface.
 *----------------------------------------------------------------------------*/
struct legion_io_policy_t {
  using hdf5_t = legion_hdf5_t;
  using hdf5_region_t = legion_hdf5_region_t;
  using launch_space_t = Legion::IndexSpace;
  using field_reference_t = data::field_reference_t;

//----------------------------------------------------------------------------//
// Implementation of legion_io_policy_t::legion_io_policy_t.
//----------------------------------------------------------------------------//   
  legion_io_policy_t() {
    file_is_map.clear();
    file_ip_map.clear();
    file_lp_map.clear();
  }

//----------------------------------------------------------------------------//
// Implementation of legion_io_policy_t::~legion_io_policy_t.
//----------------------------------------------------------------------------//   
  ~legion_io_policy_t() {
    Legion::Runtime * runtime = Legion::Runtime::get_runtime();
    Legion::Context ctx = Legion::Runtime::get_context();
    for (std::map<size_t, Legion::IndexSpace>::iterator it = file_is_map.begin();
         it != file_is_map.end(); 
         ++it) {
      if(it->second != Legion::IndexSpace::NO_SPACE) {
        //printf("clean up default_index_topology_file_is hash %ld\n", it->first);
        runtime->destroy_index_space(ctx, it->second);
      }   
    }
    file_is_map.clear();
    file_ip_map.clear();
    file_lp_map.clear();
  }

//----------------------------------------------------------------------------//
// Implementation of legion_io_policy_t::init_hdf5_file.
//----------------------------------------------------------------------------// 
  legion_hdf5_t init_hdf5_file(const char * file_name, int num_files) {
    return legion_hdf5_t(file_name, num_files);
  } // init_hdf5_file

//----------------------------------------------------------------------------//
// Implementation of legion_io_policy_t::create_hdf5_file.
//----------------------------------------------------------------------------//
  bool create_hdf5_file(legion_hdf5_t & hdf5_file, int file_idx) {
    return hdf5_file.create_hdf5_file(file_idx);
  } // create_hdf5_file

//----------------------------------------------------------------------------//
// Implementation of legion_io_policy_t::open_hdf5_file.
//----------------------------------------------------------------------------//
  bool open_hdf5_file(legion_hdf5_t & hdf5_file, int file_idx) {
    return hdf5_file.open_hdf5_file(file_idx);
  } // open_hdf5_file

//----------------------------------------------------------------------------//
// Implementation of legion_io_policy_t::close_hdf5_file.
//----------------------------------------------------------------------------//
  bool close_hdf5_file(legion_hdf5_t & hdf5_file) {
    return hdf5_file.close_hdf5_file();
  } // close_hdf5_file

//----------------------------------------------------------------------------//
// Implementation of legion_io_policy_t::create_datasets_for_regions.
//----------------------------------------------------------------------------//
  bool create_datasets_for_regions(legion_hdf5_t & hdf5_file, int file_idx) {
    return hdf5_file.create_datasets_for_regions(file_idx);
  } // create_datasets_for_regions

//----------------------------------------------------------------------------//
// Implementation of legion_io_policy_t::write_string_to_hdf5_file.
//----------------------------------------------------------------------------//
  bool write_string_to_hdf5_file(legion_hdf5_t & hdf5_file,
    int rank_id,
    const char * group_name,
    const char * dataset_name,
    const char * str,
    size_t size) {
    return hdf5_file.write_string_to_hdf5_file(
      group_name, dataset_name, str, size);
  } // write_string_to_hdf5_file

//----------------------------------------------------------------------------//
// Implementation of legion_io_policy_t::read_string_from_hdf5_file.
//----------------------------------------------------------------------------//
  bool read_string_from_hdf5_file(legion_hdf5_t & hdf5_file,
    int file_idx,
    const char * group_name,
    const char * dataset_name,
    std::string & str) {
    return hdf5_file.read_string_from_hdf5_file(group_name, dataset_name, str);
  } // read_string_from_hdf5_file

//----------------------------------------------------------------------------//
// Implementation of legion_io_policy_t::add_regions.
//----------------------------------------------------------------------------//  
  void add_regions(legion_hdf5_t & hdf5_file, std::vector<legion_hdf5_region_t> & hdf5_region_vector) {
    for(std::vector<legion_hdf5_region_t>::iterator it =
          hdf5_region_vector.begin();
        it != hdf5_region_vector.end();
        ++it) {
      hdf5_file.add_hdf5_region(*it);
    }
  } // add_regions

//----------------------------------------------------------------------------//
// Implementation of legion_io_policy_t::add_default_index_topology.
//----------------------------------------------------------------------------//  
  void add_default_index_topology(legion_hdf5_t & hdf5_file) {
    size_t identifier = flecsi_index_topology.identifier();
    auto & flecsi_context = runtime::context_t::instance();
    data::index_topo::runtime_data_t & index_runtime_data =
      flecsi_context.index_topology_instance(identifier);

    std::map<Legion::FieldID, std::string> field_string_map;

    std::vector<data::field_info_t> const & fid_vector =
      flecsi_context
        .get_field_info_store(topology::id<topology::index_topology_t>(),
          data::storage_label_t::dense)
        .field_info();

    for(std::vector<data::field_info_t>::const_iterator it = fid_vector.begin();
        it != fid_vector.end();
        ++it) {
      field_string_map[(*it).fid] = std::to_string((*it).fid);
    }

    Legion::Runtime * runtime = Legion::Runtime::get_runtime();
    Legion::Context ctx = Legion::Runtime::get_context();
    Legion::Rect<1> file_color_bounds(0, hdf5_file.num_files - 1);
    Legion::IndexSpace default_index_topology_file_is =
      runtime->create_index_space(ctx, file_color_bounds);
#if 0 
    default_index_topology_file_ip = runtime->create_pending_partition(ctx, index_runtime_data.index_space, default_index_topology_file_is);
    int idx = 0; 
    int num_subregions = index_runtime_data.colors;
    for (int point = 0; point < hdf5_file.num_files; point++) {
      std::vector<IndexSpace> subspaces;
      for (int i = 0; i < num_subregions/hdf5_file.num_files; i++) {
        subspaces.push_back(runtime->get_index_subspace(ctx, index_runtime_data.color_partition.get_index_partition(), idx));
        idx ++;
      }
      runtime->create_index_space_union(ctx, default_index_topology_file_ip, point, subspaces);
    }
#else
    Legion::IndexPartition default_index_topology_file_ip = runtime->create_equal_partition(
      ctx, index_runtime_data.index_space, default_index_topology_file_is);
#endif
    Legion::LogicalPartition default_index_topology_file_lp = runtime->get_logical_partition(
      ctx, index_runtime_data.logical_region, default_index_topology_file_ip);
    hdf5_file.add_logical_region(index_runtime_data.logical_region,
      default_index_topology_file_lp,
      "null",
      field_string_map);
    
    file_is_map[identifier] = default_index_topology_file_is;
    file_ip_map[identifier] = default_index_topology_file_ip;
    file_lp_map[identifier] = default_index_topology_file_lp;
  } // add_default_index_topology

//----------------------------------------------------------------------------//
// Implementation of legion_io_policy_t::generate_hdf5_files.
//----------------------------------------------------------------------------//  
  void generate_hdf5_files(legion_hdf5_t & hdf5_file) {
    for(int i = 0; i < hdf5_file.num_files; i++) {
      hdf5_file.create_hdf5_file(i);
      hdf5_file.create_datasets_for_regions(i);
      hdf5_file.close_hdf5_file();
    }
  } // generate_hdf5_files

//----------------------------------------------------------------------------//
// Implementation of legion_io_policy_t::checkpoint_default_index_topology.
//----------------------------------------------------------------------------//  
  void checkpoint_default_index_topology(legion_hdf5_t & hdf5_file) {
    auto & flecsi_context = runtime::context_t::instance();
    std::vector<data::field_info_t> const & fid_vector =
      flecsi_context
        .get_field_info_store(topology::id<topology::index_topology_t>(),
          data::storage_label_t::dense)
        .field_info();

    size_t identifier = flecsi_index_topology.identifier();
    {
      flog_tag_guard(io);
      flog_devel(info) << "Checkpoint default index topology, fields size "
                       << fid_vector.size()
                       << " identifier " << identifier
                       << std::endl;
    }
        
    data::index_topo::runtime_data_t & index_runtime_data =
      flecsi_context.index_topology_instance(identifier);

    legion_hdf5_region_t checkpoint_region(index_runtime_data.logical_region,
      file_lp_map[identifier],
      "default_index_topology");
    for(std::vector<data::field_info_t>::const_iterator it = fid_vector.begin();
        it != fid_vector.end();
        ++it) {
      checkpoint_region.field_string_map[(*it).fid] = std::to_string((*it).fid);
    }

    std::vector<legion_hdf5_region_t> hdf5_region_vector;
    hdf5_region_vector.push_back(checkpoint_region);
    checkpoint_data(
      hdf5_file, file_is_map[identifier], hdf5_region_vector, true);
  } // checkpoint_default_index_topology
  
  //void checkpoint_index_topology(legion_hdf5_t & hdf5_file, const flecsi::utils::const_string_t &index_topology_name);

//----------------------------------------------------------------------------//
// Implementation of legion_io_policy_t::checkpoint_default_index_topology_field.
//----------------------------------------------------------------------------//
  void checkpoint_default_index_topology_field(hdf5_t & hdf5_file, 
    const data::field_reference_t & fh) {
    auto & flecsi_context = runtime::context_t::instance();
    const data::field_info_t & fid =
      flecsi_context
        .get_field_info_store(topology::id<topology::index_topology_t>(),
          data::storage_label_t::dense)
        .get_field_info(fh.identifier());

    size_t identifier = flecsi_index_topology.identifier();

    {
      flog_tag_guard(io);
      flog_devel(info) << "Checkpoint default index topology, field "
                       << fid.fid
                       << " identifier " << identifier
                       << std::endl;
    }

    data::index_topo::runtime_data_t & index_runtime_data =
      flecsi_context.index_topology_instance(identifier);

    legion_hdf5_region_t checkpoint_region(index_runtime_data.logical_region,
      file_lp_map[identifier],
      "default_index_topology");
    checkpoint_region.field_string_map[fid.fid] = std::to_string(fid.fid);

    std::vector<legion_hdf5_region_t> hdf5_region_vector;
    hdf5_region_vector.push_back(checkpoint_region);
    checkpoint_data(
      hdf5_file, file_is_map[identifier], hdf5_region_vector, true);  
  } // checkpoint_default_index_topology_field

//----------------------------------------------------------------------------//
// Implementation of legion_io_policy_t::recover_default_index_topology.
//----------------------------------------------------------------------------//  
  void recover_default_index_topology(legion_hdf5_t & hdf5_file) {
    auto & flecsi_context = runtime::context_t::instance();
    std::vector<data::field_info_t> const & fid_vector =
      flecsi_context
        .get_field_info_store(topology::id<topology::index_topology_t>(),
          data::storage_label_t::dense)
        .field_info();

    size_t identifier = flecsi_index_topology.identifier();

    {
      flog_tag_guard(io);
      flog_devel(info) << "Recover default index topology, fields size "
                       << fid_vector.size()
                       << " identifier " << identifier
                       << std::endl;
    }
    
    data::index_topo::runtime_data_t & index_runtime_data =
      flecsi_context.index_topology_instance(identifier);

    legion_hdf5_region_t recover_region(index_runtime_data.logical_region,
      file_lp_map[identifier],
      "default_index_topology");
    for(std::vector<data::field_info_t>::const_iterator it = fid_vector.begin();
        it != fid_vector.end();
        ++it) {
      recover_region.field_string_map[(*it).fid] = std::to_string((*it).fid);
    }

    std::vector<legion_hdf5_region_t> hdf5_region_vector;
    hdf5_region_vector.push_back(recover_region);
    recover_data(
      hdf5_file, file_is_map[identifier], hdf5_region_vector, true);
  } // recover_default_index_topology

//----------------------------------------------------------------------------//
// Implementation of legion_io_policy_t::recover_default_index_topology_field.
//----------------------------------------------------------------------------//
  void recover_default_index_topology_field(hdf5_t & hdf5_file, 
    const data::field_reference_t & fh) {
    auto & flecsi_context = runtime::context_t::instance();
    const data::field_info_t & fid =
      flecsi_context
        .get_field_info_store(topology::id<topology::index_topology_t>(),
          data::storage_label_t::dense)
        .get_field_info(fh.identifier());

    size_t identifier = flecsi_index_topology.identifier();

    {
      flog_tag_guard(io);
      flog_devel(info) << "Recover default index topology, field "
                       << fid.fid
                       << " identifier " << identifier
                       << std::endl;
    }

    data::index_topo::runtime_data_t & index_runtime_data =
      flecsi_context.index_topology_instance(identifier);

    legion_hdf5_region_t recover_region(index_runtime_data.logical_region,
      file_lp_map[identifier],
      "default_index_topology");
    recover_region.field_string_map[fid.fid] = std::to_string(fid.fid);

    std::vector<legion_hdf5_region_t> hdf5_region_vector;
    hdf5_region_vector.push_back(recover_region);
    recover_data(
      hdf5_file, file_is_map[identifier], hdf5_region_vector, true);  
  } // recover_default_index_topology_field

//----------------------------------------------------------------------------//
// Implementation of legion_io_policy_t::checkpoint_data.
//----------------------------------------------------------------------------//  
  void checkpoint_data(legion_hdf5_t & hdf5_file, 
    Legion::IndexSpace launch_space, 
    std::vector<legion_hdf5_region_t> & hdf5_region_vector, 
    bool attach_flag) {
    std::string file_name = hdf5_file.file_name;
    
    Legion::Runtime * runtime = Legion::Runtime::get_runtime();
    Legion::Context ctx = Legion::Runtime::get_context();

    std::vector<std::map<Legion::FieldID, std::string>> field_string_map_vector;
    for(std::vector<legion_hdf5_region_t>::iterator it =
          hdf5_region_vector.begin();
        it != hdf5_region_vector.end();
        ++it) {
      field_string_map_vector.push_back((*it).field_string_map);
    }

    struct checkpoint_task_args_s task_argument;
    strcpy(task_argument.file_name, file_name.c_str());

    Realm::Serialization::DynamicBufferSerializer dbs(0);
    dbs << field_string_map_vector;
    task_argument.field_map_size = dbs.bytes_used();
    if(task_argument.field_map_size > SERIALIZATION_BUFFER_SIZE) {
      assert(0);
    }
    memcpy(task_argument.field_map_serial,
      dbs.detach_buffer(),
      task_argument.field_map_size);

    runtime::context_t & context_ = runtime::context_t::instance();
    auto task_id = 0;
    auto task_id_attach =
      execution::legion::task_id<checkpoint_with_attach_task, loc | inner>;
    auto task_id_without_attach =
      execution::legion::task_id<checkpoint_without_attach_task, loc | leaf>;
    
    if(attach_flag == true) {
      task_id = task_id_attach;
    }
    else {
      task_id = task_id_without_attach;
    }

    Legion::IndexLauncher checkpoint_launcher(task_id,
      launch_space,
      Legion::TaskArgument(&task_argument, sizeof(task_argument)),
      Legion::ArgumentMap());

    int idx = 0;
    for(std::vector<legion_hdf5_region_t>::iterator it =
          hdf5_region_vector.begin();
        it != hdf5_region_vector.end();
        ++it) {
      checkpoint_launcher.add_region_requirement(
        Legion::RegionRequirement((*it).logical_partition,
          0 /*projection ID*/,
          READ_ONLY,
          EXCLUSIVE,
          (*it).logical_region));

      std::map<Legion::FieldID, std::string> & field_string_map = (*it).field_string_map;
      for(std::map<Legion::FieldID, std::string>::iterator it = field_string_map.begin();
          it != field_string_map.end();
          ++it) {
        checkpoint_launcher.region_requirements[idx].add_field(it->first);
      }
      idx++;
    }
    
    {
      flog_tag_guard(io);
      flog_devel(info) << "Start checkpoint file " << file_name
                       << " regions size " << hdf5_region_vector.size()
                       << std::endl;
    }
    
    Legion::FutureMap fumap = runtime->execute_index_space(ctx, checkpoint_launcher);
    fumap.wait_all_results();
  } // checkpoint_data

//----------------------------------------------------------------------------//
// Implementation of legion_io_policy_t::recover_data.
//----------------------------------------------------------------------------//  
  void recover_data(legion_hdf5_t & hdf5_file, 
    Legion::IndexSpace launch_space, 
    std::vector<legion_hdf5_region_t> & hdf5_region_vector, 
    bool attach_flag) {
    std::string file_name = hdf5_file.file_name;

    Legion::Runtime * runtime = Legion::Runtime::get_runtime();
    Legion::Context ctx = Legion::Runtime::get_context();

    std::vector<std::map<Legion::FieldID, std::string>> field_string_map_vector;
    for(std::vector<legion_hdf5_region_t>::iterator it =
          hdf5_region_vector.begin();
        it != hdf5_region_vector.end();
        ++it) {
      field_string_map_vector.push_back((*it).field_string_map);
    }

    struct checkpoint_task_args_s task_argument;
    strcpy(task_argument.file_name, file_name.c_str());

    Realm::Serialization::DynamicBufferSerializer dbs(0);
    dbs << field_string_map_vector;
    task_argument.field_map_size = dbs.bytes_used();
    if(task_argument.field_map_size > SERIALIZATION_BUFFER_SIZE) {
      assert(0);
    }
    memcpy(task_argument.field_map_serial,
      dbs.detach_buffer(),
      task_argument.field_map_size);

    runtime::context_t & context_ = runtime::context_t::instance();
    auto task_id = 0;
    auto task_id_attach =
        execution::legion::task_id<recover_with_attach_task, loc | inner>;
    auto task_id_without_attach =
      execution::legion::task_id<recover_without_attach_task, loc | leaf>;
    
    if(attach_flag == true) {
      task_id = task_id_attach;
    }
    else {
      task_id = task_id_without_attach;
    }

    Legion::IndexLauncher recover_launcher(task_id,
      launch_space,
      Legion::TaskArgument(&task_argument, sizeof(task_argument)),
      Legion::ArgumentMap());
    int idx = 0;
    for(std::vector<legion_hdf5_region_t>::iterator it =
          hdf5_region_vector.begin();
        it != hdf5_region_vector.end();
        ++it) {
      recover_launcher.add_region_requirement(
        Legion::RegionRequirement((*it).logical_partition,
          0 /*projection ID*/,
          WRITE_DISCARD,
          EXCLUSIVE,
          (*it).logical_region));

      std::map<Legion::FieldID, std::string> & field_string_map = (*it).field_string_map;
      for(std::map<Legion::FieldID, std::string>::iterator it = field_string_map.begin();
          it != field_string_map.end();
          ++it) {
        recover_launcher.region_requirements[idx].add_field(it->first);
      }
      idx++;
    }
    
    {
      flog_tag_guard(io);
      flog_devel(info) << "Start recover file " << file_name
                       << " regions size " << hdf5_region_vector.size()
                       << std::endl;
    }

    Legion::FutureMap fumap = runtime->execute_index_space(ctx, recover_launcher);
    fumap.wait_all_results();
  } // recover_data
  
private:  
  std::map<size_t, Legion::IndexSpace> file_is_map;
  std::map<size_t, Legion::IndexPartition> file_ip_map;
  std::map<size_t, Legion::LogicalPartition> file_lp_map;
  
}; // struct legion_io_policy_t

void
checkpoint_with_attach_task(const Legion::Task * task,
  const std::vector<Legion::PhysicalRegion> & regions,
  Legion::Context ctx,
  Legion::Runtime * runtime) {
  struct checkpoint_task_args_s task_arg =
    *(struct checkpoint_task_args_s *)task->args;

  const int point = task->index_point.point_data[0];

  std::vector<std::map<Legion::FieldID, std::string>> field_string_map_vector;
  Realm::Serialization::FixedBufferDeserializer fdb(
    task_arg.field_map_serial, task_arg.field_map_size);
  bool ok = fdb >> field_string_map_vector;
  if(!ok) {
    flog(error) << "Recover task_args deserializer error" << std::endl;
  }
  
  // verification
  assert (field_string_map_vector.size() == regions.size());

  std::string fname(task_arg.file_name);
  fname = fname + std::to_string(point);
  char * file_name = const_cast<char *>(fname.c_str());

  for(unsigned int rid = 0; rid < regions.size(); rid++) {
    Legion::PhysicalRegion attach_dst_pr;
    Legion::LogicalRegion input_lr = regions[rid].get_logical_region();
    Legion::LogicalRegion attach_dst_lr = runtime->create_logical_region(
      ctx, input_lr.get_index_space(), input_lr.get_field_space());

    Legion::AttachLauncher hdf5_attach_launcher(EXTERNAL_HDF5_FILE, attach_dst_lr, attach_dst_lr);
    std::map<Legion::FieldID, const char *> field_map;
    std::set<Legion::FieldID> field_set = task->regions[rid].privilege_fields;
    std::map<Legion::FieldID, std::string>::iterator map_it;
    for(std::set<Legion::FieldID>::iterator it = field_set.begin();
        it != field_set.end();
        ++it) {
      map_it = field_string_map_vector[rid].find(*it);
      if(map_it != field_string_map_vector[rid].end()) {
        field_map.insert(std::make_pair(*it, (map_it->second).c_str()));
      }
      else {
        assert(0);
      }
    }
    
    {
      flog_tag_guard(io);
      flog_devel(info) << "Checkpoint data to HDF5 file attach "<< file_name 
                       << " region_id " << rid
                       << " (dataset(fid) size= " << field_map.size() << ")"
                       << " field_string_map_vector(regions) size " << field_string_map_vector.size()
                       << std::endl;
    }
    
    hdf5_attach_launcher.attach_hdf5(
      file_name, field_map, LEGION_FILE_READ_WRITE);
    attach_dst_pr = runtime->attach_external_resource(ctx, hdf5_attach_launcher);
    // cp_pr.wait_until_valid();

    Legion::CopyLauncher copy_launcher1;
    copy_launcher1.add_copy_requirements(
      Legion::RegionRequirement(input_lr, READ_ONLY, EXCLUSIVE, input_lr),
      Legion::RegionRequirement(attach_dst_lr, WRITE_DISCARD, EXCLUSIVE, attach_dst_lr));
    for(std::set<Legion::FieldID>::iterator it = field_set.begin();
        it != field_set.end();
        ++it) {
      copy_launcher1.add_src_field(0, *it);
      copy_launcher1.add_dst_field(0, *it);
    }
    runtime->issue_copy_operation(ctx, copy_launcher1);

    Legion::Future fu = runtime->detach_external_resource(ctx, attach_dst_pr, true);
    fu.wait();
    runtime->destroy_logical_region(ctx, attach_dst_lr);
  }
} // checkpoint_with_attach_task

void
checkpoint_without_attach_task(const Legion::Task * task,
  const std::vector<Legion::PhysicalRegion> & regions,
  Legion::Context ctx,
  Legion::Runtime * runtime) {
  struct checkpoint_task_args_s task_arg =
    *(struct checkpoint_task_args_s *)task->args;

  const int point = task->index_point.point_data[0];

  std::vector<std::map<Legion::FieldID, std::string>> field_string_map_vector;
  Realm::Serialization::FixedBufferDeserializer fdb(
    task_arg.field_map_serial, task_arg.field_map_size);
  bool ok = fdb >> field_string_map_vector;
  if(!ok) {
    flog(error) << "Recover task_args deserializer error" << std::endl;
  }
  
  // verification
  assert (field_string_map_vector.size() == regions.size());

  std::string fname(task_arg.file_name);
  fname = fname + std::to_string(point);
  char * file_name = const_cast<char *>(fname.c_str());

  hid_t file_id;
  file_id = H5Fopen(file_name, H5F_ACC_RDWR, H5P_DEFAULT);
  if(file_id < 0) {
    flog(error) << "H5Fopen failed: " << file_id << std::endl;
    assert(0);
  }

  for(unsigned int rid = 0; rid < regions.size(); rid++) {
    std::set<Legion::FieldID> field_set = task->regions[rid].privilege_fields;
    std::map<Legion::FieldID, std::string>::iterator map_it;
    for(std::set<Legion::FieldID>::iterator it = field_set.begin();
        it != field_set.end();
        ++it) {
      map_it = field_string_map_vector[rid].find(*it);
      if(map_it != field_string_map_vector[rid].end()) {
        const Legion::FieldAccessor<READ_ONLY,
          double,
          1,
          Legion::coord_t,
          Realm::AffineAccessor<double, 1, Legion::coord_t>>
          acc_fid(regions[rid], *it);
        Legion::Rect<1> rect = runtime->get_index_space_domain(
          ctx, task->regions[rid].region.get_index_space());
        const double * dset_data = acc_fid.ptr(rect.lo);
        hid_t dataset_id =
          H5Dopen2(file_id, (map_it->second).c_str(), H5P_DEFAULT);
        if(dataset_id < 0) {
          flog(error) << "H5Dopen2 failed: " << dataset_id << std::endl;
          H5Fclose(file_id);
          assert(0);
        }
        H5Dwrite(
          dataset_id, H5T_IEEE_F64LE, H5S_ALL, H5S_ALL, H5P_DEFAULT, dset_data);
        H5Dclose(dataset_id);
      }
      else {
        assert(0);
      }
    }
      
    {
      flog_tag_guard(io);
      flog_devel(info) << "Checkpoint data to HDF5 file no attach "<< file_name 
                       << " region_id " << rid
                       << " (dataset(fid) size= " << field_set.size() << ")"
                       << " field_string_map_vector(regions) size " << field_string_map_vector.size()
                       << std::endl;
    }
  }

  H5Fflush(file_id, H5F_SCOPE_LOCAL);
  H5Fclose(file_id);
} // checkpoint_without_attach_task

void
recover_with_attach_task(const Legion::Task * task,
  const std::vector<Legion::PhysicalRegion> & regions,
  Legion::Context ctx,
  Legion::Runtime * runtime) {
  const int point = task->index_point.point_data[0];

  struct checkpoint_task_args_s task_arg =
    *(struct checkpoint_task_args_s *)task->args;
  std::vector<std::map<Legion::FieldID, std::string>> field_string_map_vector;
  Realm::Serialization::FixedBufferDeserializer fdb(
    task_arg.field_map_serial, task_arg.field_map_size);
  bool ok = fdb >> field_string_map_vector;
  if(!ok) {
    flog(error) << "Recover task_args deserializer error" << std::endl;
  }
  
  // verification
  assert (field_string_map_vector.size() == regions.size());

  std::string fname(task_arg.file_name);
  fname = fname + std::to_string(point);
  char * file_name = const_cast<char *>(fname.c_str());

  for(unsigned int rid = 0; rid < regions.size(); rid++) {
    Legion::PhysicalRegion attach_src_pr;
    Legion::LogicalRegion output_lr = regions[rid].get_logical_region();
    Legion::LogicalRegion attach_src_lr = runtime->create_logical_region(
      ctx, output_lr.get_index_space(), output_lr.get_field_space());

    Legion::AttachLauncher hdf5_attach_launcher(
      EXTERNAL_HDF5_FILE, attach_src_lr, attach_src_lr);
    std::map<Legion::FieldID, const char *> field_map;
    std::set<Legion::FieldID> field_set = task->regions[rid].privilege_fields;
    std::map<Legion::FieldID, std::string>::iterator map_it;
    for(std::set<Legion::FieldID>::iterator it = field_set.begin();
        it != field_set.end();
        ++it) {
      map_it = field_string_map_vector[rid].find(*it);
      if(map_it != field_string_map_vector[rid].end()) {
        field_map.insert(std::make_pair(*it, (map_it->second).c_str()));
      }
      else {
        assert(0);
      }
    }
      
    {
      flog_tag_guard(io);
      flog_devel(info) << "Recorver data to HDF5 file attach "<< file_name 
                       << " region_id " << rid
                       << " (dataset(fid) size= " << field_map.size() << ")"
                       << " field_string_map_vector(regions) size " << field_string_map_vector.size()
                       << std::endl;
    }
      
    hdf5_attach_launcher.attach_hdf5(
      file_name, field_map, LEGION_FILE_READ_WRITE);
    attach_src_pr = runtime->attach_external_resource(ctx, hdf5_attach_launcher);

    Legion::CopyLauncher copy_launcher2;
    copy_launcher2.add_copy_requirements(
      Legion::RegionRequirement(attach_src_lr, READ_ONLY, EXCLUSIVE, attach_src_lr),
      Legion::RegionRequirement(output_lr, WRITE_DISCARD, EXCLUSIVE, output_lr));
    for(std::set<Legion::FieldID>::iterator it = field_set.begin();
        it != field_set.end();
        ++it) {
      copy_launcher2.add_src_field(0, *it);
      copy_launcher2.add_dst_field(0, *it);
    }
    runtime->issue_copy_operation(ctx, copy_launcher2);

    Legion::Future fu = runtime->detach_external_resource(ctx, attach_src_pr, true);
    fu.wait();
    runtime->destroy_logical_region(ctx, attach_src_lr);
  }
} // recover_with_attach_task

void
recover_without_attach_task(const Legion::Task * task,
  const std::vector<Legion::PhysicalRegion> & regions,
  Legion::Context ctx,
  Legion::Runtime * runtime) {
  const int point = task->index_point.point_data[0];

  struct checkpoint_task_args_s task_arg =
    *(struct checkpoint_task_args_s *)task->args;
  std::vector<std::map<Legion::FieldID, std::string>> field_string_map_vector;
  Realm::Serialization::FixedBufferDeserializer fdb(
    task_arg.field_map_serial, task_arg.field_map_size);
  bool ok = fdb >> field_string_map_vector;
  if(!ok) {
    flog(error) << "Recover task_args deserializer error" << std::endl;
  }
  
  // verification
  assert (field_string_map_vector.size() == regions.size());

  std::string fname(task_arg.file_name);
  fname = fname + std::to_string(point);
  char * file_name = const_cast<char *>(fname.c_str());

  hid_t file_id;
  file_id = H5Fopen(file_name, H5F_ACC_RDWR, H5P_DEFAULT);
  if(file_id < 0) {
    flog(error) << "H5Fopen failed: " << file_id << std::endl;
    assert(0);
  }

  for(unsigned int rid = 0; rid < regions.size(); rid++) {
    std::set<Legion::FieldID> field_set = task->regions[rid].privilege_fields;
    std::map<Legion::FieldID, std::string>::iterator map_it;
    for(std::set<Legion::FieldID>::iterator it = field_set.begin();
        it != field_set.end();
        ++it) {
      map_it = field_string_map_vector[rid].find(*it);
      if(map_it != field_string_map_vector[rid].end()) {
        const Legion::FieldAccessor<WRITE_DISCARD,
          double,
          1,
          Legion::coord_t,
          Realm::AffineAccessor<double, 1, Legion::coord_t>>
          acc_fid(regions[rid], *it);
        Legion::Rect<1> rect = runtime->get_index_space_domain(
          ctx, task->regions[rid].region.get_index_space());
        double * dset_data = acc_fid.ptr(rect.lo);
        hid_t dataset_id =
          H5Dopen2(file_id, (map_it->second).c_str(), H5P_DEFAULT);
        if(dataset_id < 0) {
          flog(error) << "H5Dopen2 failed: " << dataset_id << std::endl;
          H5Fclose(file_id);
          assert(0);
        }
        H5Dread(
          dataset_id, H5T_IEEE_F64LE, H5S_ALL, H5S_ALL, H5P_DEFAULT, dset_data);
        H5Dclose(dataset_id);
      }
      else {
        assert(0);
      }
    }
      
    {
      flog_tag_guard(io);
      flog_devel(info) << "Checkpoint data to HDF5 file no attach "<< file_name 
                       << " region_id " << rid
                       << " (dataset(fid) size= " << field_set.size() << ")"
                       << " field_string_map_vector(regions) size " << field_string_map_vector.size()
                       << std::endl;
    }
  }
} // recover_without_attach_task

} // namespace io
} // namespace flecsi