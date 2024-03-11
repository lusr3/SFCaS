cmake_minimum_required(VERSION 3.8)

project(SFCaS_DFS C CXX)

# protobuf
find_package(Protobuf CONFIG REQUIRED)
message(STATUS "Using protobuf ${Protobuf_VERSION}")
set(_PROTOBUF_LIBPROTOBUF protobuf::libprotobuf)
set(_REFLECTION gRPC::grpc++_reflection)
if(CMAKE_CROSSCOMPILING)
  find_program(_PROTOBUF_PROTOC protoc)
else()
  set(_PROTOBUF_PROTOC $<TARGET_FILE:protobuf::protoc>)
endif()

# Find gRPC installation
# Looks for gRPCConfig.cmake file installed by gRPC's cmake installation.
#查找本地gRPC模块的库信息
find_package(gRPC CONFIG REQUIRED)
message(STATUS "Using gRPC ${gRPC_VERSION}")
set(_GRPC_GRPCPP gRPC::grpc++)
if(CMAKE_CROSSCOMPILING)
  find_program(_GRPC_CPP_PLUGIN_EXECUTABLE grpc_cpp_plugin)
else()
  set(_GRPC_CPP_PLUGIN_EXECUTABLE $<TARGET_FILE:gRPC::grpc_cpp_plugin>)
endif()

# MKL 库
set(MKL_INCLUDE_DIR "/opt/intel/oneapi/mkl/2023.2.0/include")
set(MKL_LIB_DIR "/opt/intel/oneapi/mkl/2023.2.0/lib/intel64")

# FUSE 库
set(FUSE_INCLUDE_DIR "/usr/local/include/fuse3")
set(FUSE_LIBS_DIR "/usr/local/lib/x86_64-linux-gnu")

# 操作目录
set(OP_DIR "${CMAKE_SOURCE_DIR}/testDir")
set(MOUNT_DIR "${CMAKE_SOURCE_DIR}/mountDir")
set(CLIENT_DIR "${CMAKE_SOURCE_DIR}/clientDir")
set(CLIENT_MOUNT_DIR "${CMAKE_SOURCE_DIR}/clientMountDir")
message(STATUS "Actual directory: ${OP_DIR}")
message(STATUS "Mount directory ${MOUNT_DIR}")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/libs")
file(MAKE_DIRECTORY "${CMAKE_SOURCE_DIR}/bin")
file(MAKE_DIRECTORY "${CMAKE_SOURCE_DIR}/libs")
file(MAKE_DIRECTORY "${OP_DIR}")
file(MAKE_DIRECTORY "${MOUNT_DIR}")
file(MAKE_DIRECTORY "${CLIENT_DIR}")
file(MAKE_DIRECTORY "${CLIENT_MOUNT_DIR}")

# build
include_directories("${CMAKE_SOURCE_DIR}/include")
aux_source_directory("${CMAKE_SOURCE_DIR}/src/aux" AUX_SRC)
aux_source_directory("${CMAKE_SOURCE_DIR}/src/sindex" SINDEX_SRC)

# main program
add_executable(sfcas "${CMAKE_SOURCE_DIR}/src/sfcas.cpp" ${AUX_SRC} ${SINDEX_SRC})
target_link_directories(sfcas PRIVATE ${MKL_LIB_DIR} ${FUSE_LIBS_DIR})
target_compile_options(sfcas PRIVATE -Wall -fmax-errors=5 -faligned-new -march=native -mtune=native -DNDEBUGGING)
target_include_directories(sfcas PRIVATE "${CMAKE_SOURCE_DIR}/include/sindex" ${MKL_INCLUDE_DIR} ${FUSE_INCLUDE_DIR})
target_link_libraries(sfcas PRIVATE pthread mkl_rt fuse3)

add_executable(combineFile "${CMAKE_SOURCE_DIR}/src/combine/combineFile.cpp" "${CMAKE_SOURCE_DIR}/src/aux/needle.cpp")

# test program
add_executable(readFile "${CMAKE_SOURCE_DIR}/test/readFile.cpp")
add_executable(createFile "${CMAKE_SOURCE_DIR}/test/createFile.cpp")

file(MAKE_DIRECTORY "${CMAKE_SOURCE_DIR}/back")
add_executable(directCreateFile "${CMAKE_SOURCE_DIR}/test/directCreateFile.cpp" "${CMAKE_SOURCE_DIR}/src/aux/needle.cpp")

# dfs
# protobuf
get_filename_component(file_access_proto "${CMAKE_SOURCE_DIR}/src/proto/file_access.proto" ABSOLUTE)
get_filename_component(health_check_proto "${CMAKE_SOURCE_DIR}/src/proto/health_check.proto" ABSOLUTE)
get_filename_component(proto_path "${file_access_proto}" PATH)

# fileaccess service sources
set(grpc_ouput_path "${CMAKE_SOURCE_DIR}/src/grpc")
set(file_access_proto_srcs "${CMAKE_SOURCE_DIR}/src/grpc/file_access.pb.cc")
set(file_access_grpc_hdrs "${CMAKE_SOURCE_DIR}/src/grpc/file_access.pb.h")
set(file_access_grpc_srcs "${CMAKE_SOURCE_DIR}/src/grpc/file_access.grpc.pb.cc")
set(file_access_grpc_hdrs "${CMAKE_SOURCE_DIR}/src/grpc/file_access.grpc.pb.h")
add_custom_command(
    OUTPUT "${file_access_proto_srcs}" "${file_access_proto_hdrs}" "${file_access_grpc_srcs}" "${file_access_grpc_hdrs}"
    COMMAND ${_PROTOBUF_PROTOC}
    ARGS --grpc_out "${grpc_ouput_path}"
        --cpp_out "${grpc_ouput_path}"
        -I "${proto_path}"
        --plugin=protoc-gen-grpc="${_GRPC_CPP_PLUGIN_EXECUTABLE}"
        "${file_access_proto}"
    DEPENDS "${file_access_proto}")

# health check service sources
set(grpc_ouput_path "${CMAKE_SOURCE_DIR}/src/grpc")
set(health_check_proto_srcs "${CMAKE_SOURCE_DIR}/src/grpc/health_check.pb.cc")
set(health_check_grpc_hdrs "${CMAKE_SOURCE_DIR}/src/grpc/health_check.pb.h")
set(health_check_grpc_srcs "${CMAKE_SOURCE_DIR}/src/grpc/health_check.grpc.pb.cc")
set(health_check_grpc_hdrs "${CMAKE_SOURCE_DIR}/src/grpc/health_check.grpc.pb.h")
add_custom_command(
    OUTPUT "${health_check_proto_srcs}" "${health_check_proto_hdrs}" "${health_check_grpc_srcs}" "${health_check_grpc_hdrs}"
    COMMAND ${_PROTOBUF_PROTOC}
    ARGS --grpc_out "${grpc_ouput_path}"
        --cpp_out "${grpc_ouput_path}"
        -I "${proto_path}"
        --plugin=protoc-gen-grpc="${_GRPC_CPP_PLUGIN_EXECUTABLE}"
        "${health_check_proto}"
    DEPENDS "${health_check_proto}")

add_library(file_access_grpc_proto
    ${file_access_grpc_srcs}
    ${file_access_grpc_hdrs}
    ${file_access_proto_srcs}
    ${file_access_proto_hdrs})
target_link_libraries(file_access_grpc_proto
    ${_REFLECTION}
    ${_GRPC_GRPCPP}
    ${_PROTOBUF_LIBPROTOBUF})

add_library(health_check_grpc_proto
    ${health_check_grpc_srcs}
    ${health_check_grpc_hdrs}
    ${health_check_proto_srcs}
    ${health_check_proto_hdrs})
target_link_libraries(health_check_grpc_proto
    ${_REFLECTION}
    ${_GRPC_GRPCPP}
    ${_PROTOBUF_LIBPROTOBUF})
  
# nameserver dataserver client
add_executable(nameserver "${CMAKE_SOURCE_DIR}/src/dfs/nameserver.cc")
target_include_directories(nameserver PRIVATE "${CMAKE_SOURCE_DIR}/src/grpc" "${CMAKE_SOURCE_DIR}/include/dfs")
target_link_libraries(nameserver file_access_grpc_proto health_check_grpc_proto)

add_executable(dataserver 
  "${CMAKE_SOURCE_DIR}/src/dfs/dataserver.cc"
  "${CMAKE_SOURCE_DIR}/src/aux/needle.cpp")
target_include_directories(dataserver PRIVATE "${CMAKE_SOURCE_DIR}/src/grpc" "${CMAKE_SOURCE_DIR}/include/dfs")
target_link_libraries(dataserver file_access_grpc_proto health_check_grpc_proto)

add_executable(client "${CMAKE_SOURCE_DIR}/src/dfs/client.cc")
target_include_directories(client PRIVATE "${CMAKE_SOURCE_DIR}/src/grpc" "${FUSE_INCLUDE_DIR}" "${CMAKE_SOURCE_DIR}/include/dfs")
target_link_directories(client PRIVATE ${FUSE_LIBS_DIR})
target_link_libraries(client PRIVATE fuse3 file_access_grpc_proto)