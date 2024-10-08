#ifndef NODEREGIST_H
#define NODEREGIST_H
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <atomic>
#include <glog/logging.h>

#include <thread>
#include <functional>
#include <condition_variable>

#include <grpcpp/ext/proto_server_reflection_plugin.h>
#include <grpcpp/grpcpp.h>
#include <grpcpp/health_check_service_interface.h>

#include "registrar.grpc.pb.h"
#include "exception.hpp"

namespace core {
    class NodeHandler;
    using namespace std;
    using namespace grpc;
    using namespace registrar;
    
    class NodeRegist {
        public:
        NodeRegist(const string& node_name, shared_ptr<core::NodeHandler> nh) ;
        ~NodeRegist();
        private:
        unique_ptr<registrar::Registration::Stub>   register_stub_;
        thread                                      register_thread;
        shared_ptr<core::NodeHandler>               nh_;
        const string                                this_node_name;
    };
} 


#endif