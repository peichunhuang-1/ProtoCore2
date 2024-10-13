#ifndef NODECOMMAND_HPP
#define NODECOMMAND_HPP
#include <algorithm>
#include "cmd.grpc.pb.h"
#include <shared_mutex>
#include <glog/logging.h>
namespace core {
using namespace grpc;
using namespace cmd;
using namespace std;
class NodeCommandServerImpl final :public NodeCommand::Service {
    public:
    NodeCommandServerImpl();
    Status                                  getInfo(ServerContext* context, 
                                            const getInfoRequest*request, grpc::ServerWriter<getInfoReply>* writer);
    Status                                  kill(ServerContext* context, 
                                            const google::protobuf::Empty*request, const google::protobuf::Empty* reply);                                      
    void                                    setTopic(const string& topic);
    void                                    setService(const string& service);
    void                                    setParam(const string& param);
    
    private:
    vector<string>                          topics;
    vector<string>                          services;
    vector<string>                          params;
    condition_variable                      cv;
    mutex                                   cv_mtx;
    shared_mutex                            mtx;

    int                                     get_version(cmd::getInfoRequest::InfoType type);
};
}

#endif