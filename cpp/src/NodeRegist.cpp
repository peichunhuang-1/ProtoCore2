#include "NodeRegist.hpp"
#include "rscl.hpp"
namespace core
{
    NodeRegist::NodeRegist(const string& node_name, shared_ptr<core::NodeHandler> nh): nh_(nh), this_node_name(node_name), 
    register_stub_(Registration::NewStub(CreateChannel(string(getenv("CORE_MASTER_ADDR") ? getenv("CORE_MASTER_ADDR") : DEFAULT_CORE_MASTER_ADR), InsecureChannelCredentials()))) {
        LOG(INFO) << "Create node register...";
        register_thread = thread([this] () {
            LOG(INFO) << "Start node register thread...";
            RegistrationRequest request;
            request.mutable_info()->set_node_name(this_node_name);
            request.mutable_info()->set_ip(string(getenv("CORE_LOCAL_IP") ? getenv("CORE_LOCAL_IP") : DEFAULT_CORE_LOCAL_IP));
            request.mutable_info()->set_port(nh_->this_node_connection_rpc_port);
            LOG(INFO) << "Trying to establish connection between registrar...";
            ClientContext context;
            shared_ptr<ClientReader<RegistrationReply> > stream(
                this->register_stub_->Regist(&context, request));
            RegistrationReply response;
            while (stream->Read(&response) && core::ok()) {
                LOG(INFO) << "Receive message from registrar...";
                int opcmd_size = response.operation_cmd().size();
                for (int i = 0; i < opcmd_size; i++) {
                    auto res = response.operation_cmd(i);
                    if (res.code() == NodeInfo::ADD) {
                        nh_->regist_node(res.node_name(), res.ip(), res.port());
                    } else {
                        nh_->delete_node(res.node_name());
                    }
                }
            }
            LOG(ERROR) << "Connection loss: registrar may be closed";
        });
    }

    NodeRegist::~NodeRegist(){
        register_thread.join();
    }
}