#ifndef SERVICE_CLIENT_HPP
#define SERVICE_CLIENT_HPP
#include "service.grpc.pb.h"

namespace core {

class StreamCloseException : public std::exception {
public:
    const char* what() const noexcept override {
        return "serive stream has been closed.";
    }
};

template<typename Request, typename Reply>
class ServiceClient {
    using Stream = unique_ptr<ClientReaderWriter<ServiceRPCRequest, ServiceRPCReply> >;
    using ClientStub = unique_ptr<service_rpc::ServiceRPC::Stub>;
    public:
    ServiceClient(const string& service, future<string>& received_server_request, shared_ptr<core::NodeHandler> nh) ;
    bool                                request(const Request& request, future<Reply>& reply);
    bool                                cancel();
    ServiceCallStatus                   ServerStatus();
    void                                reset();

    private:
    ServiceCallStatus                   status;
    shared_mutex                        status_mtx;
    shared_ptr<core::NodeHandler>       nh_;
    const string                        service_name;
    unique_ptr<ClientContext>           context_;
    Stream                              stream_;
    ClientStub                          stub_;
    shared_ptr<grpc::Channel>           channel;
    thread                              wait_server_notify_thread;
    atomic<bool>                        connected;  
    atomic<bool>                        requested;  
    promise<Reply>                      reply_promise;

    shared_mutex                        usr_operation_mtx;

    future<string>                      wait_server_addr;
    ServiceRPCRequest                   request_;
    ServiceRPCReply                     response_; 
    grpc::Status                        rpc_status;
    void                                create_connection(const string& srv_addr);
};

template<typename Request, typename Reply>
ServiceClient<Request, Reply>::ServiceClient(const string& service, future<string>& received_server_request, shared_ptr<core::NodeHandler> nh) 
: nh_(nh), service_name(service), wait_server_addr(move(received_server_request)) {
    connected = false;
    wait_server_notify_thread = thread([this]() mutable {
        const string srv_addr = wait_server_addr.get();
        create_connection(srv_addr);
    });
    
}
template<typename Request, typename Reply>
ServiceCallStatus ServiceClient<Request, Reply>::ServerStatus() {
    shared_lock<shared_mutex> lock(status_mtx);
    return status;
}
template<typename Request, typename Reply>
bool ServiceClient<Request, Reply>::cancel() {
    unique_lock<shared_mutex> lock(usr_operation_mtx);
    if (!connected.load() || !requested.load()) return false;
    ServiceRPCRequest cancel_call;
    cancel_call.set_setting(ServiceRPCRequest::SET_CANCELED);
    if (stream_->Write(cancel_call)) {
        return true;
    }
    wait_server_notify_thread.join();
    return false;
}
template<typename Request, typename Reply>
bool ServiceClient<Request, Reply>::request(const Request& request, future<Reply>& reply) {
    unique_lock<shared_mutex> lock(usr_operation_mtx);
    if (!connected.load() || requested.load()) return false;
    request_.set_setting(ServiceRPCRequest::PULL_NEW_REQ);
    request_.mutable_payload()->PackFrom(request);
    if (stream_->Write(request_)) {
        requested = true;
        reply = move(reply_promise.get_future());
        return true;
    }
    wait_server_notify_thread.join();
    return false;
}

template<typename Request, typename Reply>
void ServiceClient<Request, Reply>::create_connection(const string& srv_addr) {
    context_ = make_unique<ClientContext>();
    channel = CreateChannel(srv_addr, InsecureChannelCredentials());
    stub_ = service_rpc::ServiceRPC::NewStub(channel);
    stream_ = stub_->ServicePullRequest(context_.get());
    reply_promise = promise<Reply>();
    connected = true;
    requested = false;
    while (core::ok()) {
        if (stream_->Read(&response_)) {
            unique_lock<shared_mutex> lock(status_mtx);
            status = response_.status();
            if (status == ServiceRPCReply::SUCCESS || 
            status == ServiceRPCReply::FAILED || 
            status == ServiceRPCReply::ABORTED) {
                Reply reply;
                response_.payload().UnpackTo(&reply);
                reply_promise.set_value(reply);
                reply_promise = promise<Reply>();
                requested = false;
            }
        } else {
            LOG(INFO) << "service server stream closed";
            if (requested.load()) {
                reply_promise.set_exception(make_exception_ptr(StreamCloseException()));
            }
            reply_promise = promise<Reply>();
            connected = false;
            requested = false;
            return ;
        }
    }
}

}

#endif