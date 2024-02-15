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
message(STATUS "Actual directory: ${OP_DIR}")
message(STATUS "Mount directory ${MOUNT_DIR}")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/libs")
file(MAKE_DIRECTORY "${CMAKE_SOURCE_DIR}/bin")
file(MAKE_DIRECTORY "${CMAKE_SOURCE_DIR}/libs")
file(MAKE_DIRECTORY "${OP_DIR}")
file(MAKE_DIRECTORY "${MOUNT_DIR}")

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
get_filename_component(sfcas_proto "${CMAKE_SOURCE_DIR}/src/proto/sfcas.proto" ABSOLUTE)
get_filename_component(sfcas_proto_path "${sfcas_proto}" PATH)

# Generated sources
set(sfcas_proto_srcs "${CMAKE_SOURCE_DIR}/src/proto/sfcas.pb.cc")
set(sfcas_proto_hdrs "${CMAKE_SOURCE_DIR}/src/proto/sfcas.pb.h")
set(sfcas_grpc_srcs "${CMAKE_SOURCE_DIR}/src/proto/sfcas.grpc.pb.cc")
set(sfcas_grpc_hdrs "${CMAKE_SOURCE_DIR}/src/proto/sfcas.grpc.pb.h")
add_custom_command(
    OUTPUT "${sfcas_proto_srcs}" "${sfcas_proto_hdrs}" "${sfcas_grpc_srcs}" "${sfcas_grpc_hdrs}"
    COMMAND ${_PROTOBUF_PROTOC}
    ARGS --grpc_out "."
        --cpp_out "."
        -I "${sfcas_proto_path}"
        --plugin=protoc-gen-grpc="${_GRPC_CPP_PLUGIN_EXECUTABLE}"
        "${sfcas_proto}"
    DEPENDS "${sfcas_proto}"
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/src/proto")

add_library(sfcas_grpc_proto
    ${sfcas_grpc_srcs}
    ${sfcas_grpc_hdrs}
    ${sfcas_proto_srcs}
    ${sfcas_proto_hdrs})
target_link_libraries(sfcas_grpc_proto
    ${_REFLECTION}
    ${_GRPC_GRPCPP}
    ${_PROTOBUF_LIBPROTOBUF})