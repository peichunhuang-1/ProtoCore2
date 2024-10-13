#include "NodeCommand.hpp"
#include "common.hpp"
namespace core {

NodeCommandServerImpl::NodeCommandServerImpl() {}

Status NodeCommandServerImpl::getInfo(ServerContext* context, 
    const getInfoRequest*request, grpc::ServerWriter<getInfoReply>* writer) {
    int topic_version_pointer = 0;
    int service_version_pointer = 0;
    int param_version_pointer = 0;
    while (core::ok()) {
        int topic_version = get_version(getInfoRequest::InfoType((int) request->type() & getInfoRequest::TOPIC));
        int service_version = get_version(getInfoRequest::InfoType((int) request->type() & getInfoRequest::SERVICE));
        int param_version = get_version(getInfoRequest::InfoType((int) request->type() & getInfoRequest::PARAM));
        getInfoReply reply;
        for (int i = topic_version_pointer; i < topic_version; i++) {
            shared_lock<shared_mutex> lock(mtx);
            reply.add_topics(topics[i]);
        }
        for (int i = service_version_pointer; i < service_version; i++) {
            shared_lock<shared_mutex> lock(mtx);
            reply.add_services(services[i]);
        }
        for (int i = param_version_pointer; i < param_version; i++) {
            shared_lock<shared_mutex> lock(mtx);
            reply.add_params(params[i]);
        }
        if (topic_version != topic_version_pointer || 
            service_version != service_version_pointer || 
            param_version != param_version_pointer) {
            topic_version_pointer = topic_version ;
            service_version_pointer = service_version ;
            param_version_pointer = param_version ;
            writer->Write(reply);
        }
        unique_lock<mutex> cv_lock(cv_mtx);
        cv.wait(cv_lock);
    }
}

int NodeCommandServerImpl::get_version(getInfoRequest::InfoType type) {
    shared_lock<shared_mutex> lock(mtx);
    switch (type) {
        case getInfoRequest::TOPIC:
            return topics.size();
        break;
        case getInfoRequest::SERVICE:
            return services.size();
        break;
        case getInfoRequest::PARAM:
            return params.size();
        break;
        case getInfoRequest::NONE:
            return 0;
        break;
        default:
            return 0;
        break;
    }
}


void NodeCommandServerImpl::setTopic(const string& topic) {
    unique_lock<shared_mutex> lock(mtx);
    auto it = find(topics.begin(), topics.end(), topic);
    if (it == topics.end()) {
        topics.push_back(topic);
        cv.notify_all();
    }
}

void NodeCommandServerImpl::setService(const string& service) {
    unique_lock<shared_mutex> lock(mtx);
    auto it = find(services.begin(), services.end(), service);
    if (it == services.end()) {
        services.push_back(service);
        cv.notify_all();
    }
}

void NodeCommandServerImpl::setParam(const string& param) {
    unique_lock<shared_mutex> lock(mtx);
    auto it = find(params.begin(), params.end(), param);
    if (it == params.end()) {
        params.push_back(param);
        cv.notify_all();
    }
}

Status NodeCommandServerImpl::kill(ServerContext* context, 
const google::protobuf::Empty*request, const google::protobuf::Empty* reply) {
    core::setAbort();
    LOG(INFO) << "Killed exit...";
    exit(0);
}  
}