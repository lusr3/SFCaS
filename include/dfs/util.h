#include <iostream>
#include <string>
#include <unordered_map>
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <algorithm>

#include <grpcpp/grpcpp.h>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/ini_parser.hpp>

#include "file_access.grpc.pb.h"
#include "health_check.grpc.pb.h"

#if !defined(UTIL_H)
#define UTIL_H

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;
using grpc::StatusCode;
using grpc::ServerReader;
using grpc::Channel;
using grpc::ClientContext;
using google::protobuf::Empty;
using grpc::ServerWriter;
using grpc::ClientWriter;
using grpc::ClientReader;

using std::string;
using std::vector;
using std::unordered_map;
using std::unique_ptr;
using std::shared_ptr;
using std::mutex;
using std::shared_mutex;
using std::atomic;
using std::thread;
using std::cout;
using std::endl;

using sfcas::fileaccess::FileAccess;
using sfcas::fileaccess::MetaDataRequest;
using sfcas::fileaccess::MetaDataReply;
using sfcas::fileaccess::DataRequest;
using sfcas::fileaccess::DataReply;
using sfcas::fileaccess::StartUpMsg;
using sfcas::fileaccess::StartUpReply;
using sfcas::healthcheck::HealthCheck;
using sfcas::healthcheck::HealthCheckRequest;
using sfcas::healthcheck::HealthCheckResponse;

typedef std::chrono::high_resolution_clock Clock;
typedef std::chrono::time_point<Clock> Timestamp;

#endif