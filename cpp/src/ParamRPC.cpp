#include "ParamRPC.hpp"
namespace core {
Status ParamRPCServerImpl::getRemoteParam(ServerContext* context, 
    const getParamRPCRequest*request, ServerWriter<getParamRPCReply>* writer) {
    const string name = request->name();
    int version = 0;
    while (core::ok() && !context->IsCancelled()) {
        getParamRPCReply reply;
        if (find_param(name)) {
            version = params_version[name].load();
            reply.set_status(ParameterStatus::NORMAL);
            shared_lock<shared_mutex> lock(params_mtx);
            reply.mutable_payload()->CopyFrom(params[name]->getRemoteValue());
        } else {
            reply.set_status(ParameterStatus::TMEP_NOT_AVALIABLE);
        }
        writer->Write(reply);
        unique_lock<mutex> lock(mtx);
        cv.wait(lock, [&]() {
            return (find_param(name) && version != params_version[name].load()) || !core::ok();
        });
    }
    return Status::OK;
}

Status ParamRPCServerImpl::setRemoteParam(ServerContext* context, 
    const setParamRPCRequest*request, setParamRPCReply* reply) {
    const string name = request->name();
    if (is_const(name)) {
        reply->set_status(ParameterStatus::NOT_CHANGEABLE);
    } else if (!find_param(name)) {
        reply->set_status(ParameterStatus::TMEP_NOT_AVALIABLE);
    } else {
        unique_lock<shared_mutex> lock(params_mtx);
        params[name]->setRemoteValue(request->payload());
        reply->set_status(ParameterStatus::NORMAL);
        params_version[name]++;
        cv.notify_all();
    }
    return Status::OK;
}
bool ParamRPCServerImpl::find_param(const string& name) {
    shared_lock<shared_mutex> lock(params_mtx);
    return params.count(name);
}

bool ParamRPCServerImpl::is_const(const string& name) {
    shared_lock<shared_mutex> lock(is_const_params_mtx);
    return is_const_params.count(name);
}

void ParamSlot::get(getParamRPCReply& reply) {
    reply = data;
}
void ParamSlot::set(const getParamRPCReply& reply) {
    data = reply;
}

void DynamicParamSlot::set(const getParamRPCReply& reply) {
    data = reply;
    if (data.status() == ParameterStatus::NORMAL) {
        thread([this, reply](){
            cb(reply);
        }).detach();
    }
}

void DynamicParamSlot::get(getParamRPCReply& reply) {
    reply = data;
}

ParamRPCClient::ParamRPCClient(shared_ptr<Channel> channel): stub_(ParamRPC::NewStub(channel)) {}

ParamRPCClient::~ParamRPCClient() {
    unique_lock<shared_mutex> lock(write_thread_mtx);
    while (!write_thread.empty()) {
        write_thread.front().join();
        write_thread.pop();
    }
}

bool ParamRPCClient::find_param(const string& name) {
    shared_lock<shared_mutex> lock(get_replies_mtx);
    return get_replies.count(name);
}

setParamRPCReply ParamRPCClient::setRemoteParam(const setParamRPCRequest& request) {
    ClientContext context;
    setParamRPCReply reply;
    Status status = stub_->setRemoteParam(&context, request, &reply);
    if (!status.ok()) {
        reply.set_status(ParameterStatus::TMEP_NOT_AVALIABLE);
    } 
    return reply;
}

void ParamRPCClient::getRemoteParam(const getParamRPCRequest& request, getParamRPCReply& reply, function<void(const getParamRPCReply)> cb) {
    reply.set_status(ParameterStatus::TMEP_NOT_AVALIABLE);
    if (find_param(request.name())) {
        shared_lock<shared_mutex> lock(get_replies_mtx);
        if (cb) get_replies[request.name()] = static_cast<ParamSlot*>( new DynamicParamSlot(cb) );
        return get_replies[request.name()]->get(reply);
    }
    unique_lock<shared_mutex> lock(get_replies_mtx);
    if (cb) get_replies[request.name()] = static_cast<ParamSlot*>( new DynamicParamSlot(cb) );
    else get_replies[request.name()] = new ParamSlot();
    lock.unlock();
    unique_lock<shared_mutex> thread_lock(write_thread_mtx);
    write_thread.push(
        thread( [this, request](){
        ClientContext context;
        getParamRPCReply response;
        unique_ptr<ClientReader<getParamRPCReply>> reader = stub_->getRemoteParam(&context, request);
        while (reader->Read(&response)) {
            unique_lock<shared_mutex> lock(get_replies_mtx);
            get_replies[request.name()]->set(response);
        }
        })
    );
}

ParamRPCClientClub::ParamRPCClientClub() {}

void ParamRPCClientClub::add_client(const string& namespace_, const string& srv_addr) {
    unique_lock<shared_mutex> lock(mtx);
    clients[namespace_] = make_unique<ParamRPCClient>( CreateChannel(srv_addr, grpc::InsecureChannelCredentials()) );
    auto req_pack_queue = get_requests_queue[namespace_];
    while (!req_pack_queue.empty()) {
        auto req_pack = req_pack_queue.front();
        getParamRPCReply reply;
        clients[namespace_]->getRemoteParam(req_pack.req, reply, req_pack.cb);
        req_pack_queue.pop();
    }
}

void ParamRPCClientClub::delete_client(const string& namespace_) {
    unique_lock<shared_mutex> lock(mtx);
    clients.erase(namespace_);
}

}