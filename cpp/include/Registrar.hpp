#ifndef REGISTRAR_H
#define REGISTRAR_H
#include <unordered_set>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <atomic>
#include <glog/logging.h>
#include <string.h>

#include <thread>
#include <functional>
#include <condition_variable>

#include <grpcpp/ext/proto_server_reflection_plugin.h>
#include <grpcpp/grpcpp.h>
#include <grpcpp/health_check_service_interface.h>

#include "registrar.grpc.pb.h"
#include "exception.hpp"

namespace core {
    using namespace std;
    using namespace grpc;
    using namespace registrar;

    class Registrar {
        public:
        Registrar();
        const int                               registration(const int& fixed_version, const string& name, const string& ip, const int& port);
        void                                    deregistration(const int& version);
        RegistrationReply                       list(const int& from, const int& to);
        bool                                    find_exist_node(const string& name);
        private:
        atomic<int>                             version;
        vector<pair<int, NodeInfo>>             nodes;
        shared_mutex                            mtx;
        unordered_set<string>                   alive_nodes;
        friend class                            RegistServiceImpl;
    };
    
    class RegistServiceImpl final : public Registration::Service {
        public:
        Registrar                               registrar;
        Status                                  Regist(ServerContext* context, const RegistrationRequest* request,
                                                    grpc::ServerWriter<RegistrationReply>* writer) ;
        void                                    Healthy_Check();
        condition_variable                      cv;
        mutex                                   mtx;
        const int                               healthy_check_sec = 1;
    };

    void RunServer(string server_address);
}

#endif