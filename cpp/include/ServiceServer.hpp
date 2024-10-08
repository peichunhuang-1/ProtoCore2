#include "service.grpc.pb.h"
#include <queue>
#include <future>
namespace core {
struct ServerServiceType {
    enum CompletionQueueType { READ = 1, WRITE = 2, BROKEN_PIPE = -1, CONNECT = 0};
    int                                 stream_id;
    CompletionQueueType                 type;
};
using service_rpc::ServiceRPCReply;
using service_rpc::ServiceRPCRequest;
using ServiceCallStatus = service_rpc::ServiceRPCReply_ServiceRPCStatus;
template<typename Request, typename Reply>
class ServiceServer final : public service_rpc::ServiceRPC::AsyncService {
    public:
    using FunctionType = void(*)(const Request*, Reply*, ServiceServer<Request, Reply>*, const int&);
    using Stream = unique_ptr<ServerAsyncReaderWriter<ServiceRPCReply, ServiceRPCRequest> >;
    ServiceServer() = default;
    ServiceServer(const string& service, const string& ip, int& rpc_port, FunctionType cb, shared_ptr<core::NodeHandler> nh);
    bool                                valid();
    bool                                isPrompted(const int& stream_id);
    bool                                isAborted(const int& stream_id);
    void                                setAborted(const int& stream_id);
    void                                setSuccess(const int& stream_id);
    void                                setFailed(const int& stream_id);
    private:
    shared_ptr<core::NodeHandler>       nh_;
    const string                        service_name;
    unique_ptr<ServerCompletionQueue>   cq_;
    unique_ptr<grpc::Server>            server_;
    void                                write_reply(const ServiceRPCReply& reply, const int& stream_id);
    void                                CreateListenPort(int stream_id);
    int                                 generate_stream_id();
    void                                handle_event(const ServerServiceType* tag);
    vector<Stream>                      streams_;
    atomic<int>                         stream_id;
    queue<int>                          reusable_id;
    vector<unique_ptr<ServerContext>>   contexts_;
    vector<ServiceRPCRequest>           requests_;
    FunctionType                        cb_func;
    vector<ServiceCallStatus>           calls_status;
    shared_mutex                        calls_status_mtx;
    vector<future<void>>                future_storages;
    vector<queue<ServiceRPCReply>>      reply_wait_queue;
    shared_mutex                        reply_wait_queue_mtx;
    bool                                valid_ = false;
    thread                              handle_event_thread;

    bool                                isSuspend(const int& stream_id);
    bool                                isProcess(const int& stream_id);
    void                                setProcess(const int& stream_id);
    void                                setPrompted(const int& stream_id);
    void                                setSuspend(const int& stream_id);
    ServiceCallStatus                   endStatus(const int& stream_id);
};

template<typename Request, typename Reply>
bool ServiceServer<Request, Reply>::valid() {return valid_;}

template<typename Request, typename Reply>
bool ServiceServer<Request, Reply>::isSuspend(const int& stream_id) {
    shared_lock<shared_mutex> lock(calls_status_mtx);
    return calls_status[stream_id] == ServiceRPCReply::SUSPEND ;
}

template<typename Request, typename Reply>
bool ServiceServer<Request, Reply>::isPrompted(const int& stream_id) {
    shared_lock<shared_mutex> lock(calls_status_mtx);
    return calls_status[stream_id] == ServiceRPCReply::PROMPTED ;
}

template<typename Request, typename Reply>
bool ServiceServer<Request, Reply>::isAborted(const int& stream_id) {
    shared_lock<shared_mutex> lock(calls_status_mtx);
    return calls_status[stream_id] == ServiceRPCReply::ABORTED ;
}

template<typename Request, typename Reply>
bool ServiceServer<Request, Reply>::isProcess(const int& stream_id) {
    shared_lock<shared_mutex> lock(calls_status_mtx);
    return calls_status[stream_id] == ServiceRPCReply::PROCESS;
}

template<typename Request, typename Reply>
void ServiceServer<Request, Reply>::setProcess(const int& stream_id) {
    if (!isSuspend(stream_id)) return;
    unique_lock<shared_mutex> lock(calls_status_mtx);
    calls_status[stream_id] = ServiceRPCReply::PROCESS;
    ServiceRPCReply reply;
    reply.set_status(ServiceRPCReply::PROCESS);
    write_reply(reply, stream_id);
}

template<typename Request, typename Reply>
void ServiceServer<Request, Reply>::setPrompted(const int& stream_id) {
    if (!isProcess(stream_id)) return;
    unique_lock<shared_mutex> lock(calls_status_mtx);
    calls_status[stream_id] = ServiceRPCReply::PROMPTED;
    ServiceRPCReply reply;
    reply.set_status(ServiceRPCReply::PROMPTED);
    write_reply(reply, stream_id);
}

template<typename Request, typename Reply>
void ServiceServer<Request, Reply>::setFailed(const int& stream_id) {
    if (!isProcess(stream_id) && !isPrompted(stream_id)) return;
    unique_lock<shared_mutex> lock(calls_status_mtx);
    calls_status[stream_id] = ServiceRPCReply::FAILED;
}

template<typename Request, typename Reply>
void ServiceServer<Request, Reply>::setSuccess(const int& stream_id) {
    if (!isProcess(stream_id) && !isPrompted(stream_id)) return;
    unique_lock<shared_mutex> lock(calls_status_mtx);
    calls_status[stream_id] = ServiceRPCReply::SUCCESS;
}

template<typename Request, typename Reply>
void ServiceServer<Request, Reply>::setAborted(const int& stream_id) {
    if (!isProcess(stream_id) && !isPrompted(stream_id)) return;
    unique_lock<shared_mutex> lock(calls_status_mtx);
    calls_status[stream_id] = ServiceRPCReply::ABORTED;
}

template<typename Request, typename Reply>
void ServiceServer<Request, Reply>::setSuspend(const int& stream_id) {
    if (isProcess(stream_id) || isPrompted(stream_id)) return;
    unique_lock<shared_mutex> lock(calls_status_mtx);
    calls_status[stream_id] = ServiceRPCReply::SUSPEND;
}

template<typename Request, typename Reply>
ServiceCallStatus ServiceServer<Request, Reply>::endStatus(const int& stream_id) {
    unique_lock<shared_mutex> lock(calls_status_mtx);
    if (calls_status[stream_id] == ServiceRPCReply::SUSPEND ||
    calls_status[stream_id] == ServiceRPCReply::PROCESS || 
    calls_status[stream_id] == ServiceRPCReply::PROMPTED) {
        LOG(WARNING) << "service call back function return, but status was not manually set, auto set to aborted and reply";
        calls_status[stream_id] = ServiceRPCReply::ABORTED;
        return calls_status[stream_id];
    } else {
        return calls_status[stream_id];
    }
}

template<typename Request, typename Reply>
void ServiceServer<Request, Reply>::CreateListenPort(int stream_id) {
    if (streams_.size() <= stream_id) {
        contexts_.push_back( make_unique<ServerContext>());
        streams_.push_back(make_unique<ServerAsyncReaderWriter<ServiceRPCReply, ServiceRPCRequest>>
        (ServerAsyncReaderWriter<ServiceRPCReply, ServiceRPCRequest>(contexts_[stream_id].get())));
        requests_.push_back(ServiceRPCRequest());
        calls_status.push_back(ServiceRPCReply::SUSPEND);
        future_storages.push_back(async([]{}));
        reply_wait_queue.push_back(queue<ServiceRPCReply>());
    }

    setAborted(stream_id);
    future_storages[stream_id].get();
    contexts_[stream_id] = make_unique<ServerContext>();
    streams_[stream_id].reset( new ServerAsyncReaderWriter<service_rpc::ServiceRPCReply, service_rpc::ServiceRPCRequest>
    (contexts_[stream_id].get()) );
    this->RequestServicePullRequest(contexts_[stream_id].get(), streams_[stream_id].get(), cq_.get(), cq_.get(),
        new ServerServiceType{stream_id, ServerServiceType::CompletionQueueType::CONNECT});
    contexts_[stream_id]->AsyncNotifyWhenDone( 
        new ServerServiceType{stream_id, ServerServiceType::CompletionQueueType::BROKEN_PIPE});
}

template<typename Request, typename Reply>
int ServiceServer<Request, Reply>::generate_stream_id() {
    int id;
    if (!reusable_id.empty()) {
        id = reusable_id.front();
        reusable_id.pop();
    }
    else id = stream_id++;
    return id;
}
template<typename Request, typename Reply>
void ServiceServer<Request, Reply>::write_reply(const ServiceRPCReply& reply, const int& stream_id) {
    unique_lock<shared_mutex> lock(reply_wait_queue_mtx);
    if (contexts_[stream_id]->IsCancelled()) return;
    if (reply_wait_queue[stream_id].empty()) {
        streams_[stream_id]->Write(reply, new ServerServiceType{stream_id, ServerServiceType::CompletionQueueType::WRITE});
    } else {
        reply_wait_queue[stream_id].push(reply);
    }
}
template<typename Request, typename Reply>
void ServiceServer<Request, Reply>::handle_event(const ServerServiceType* tag) {
    switch (tag->type)
    {
    case ServerServiceType::CompletionQueueType::CONNECT:{
        int id = generate_stream_id();
        CreateListenPort(id);
        setSuspend(tag->stream_id);
        streams_[tag->stream_id]->Read(&requests_[tag->stream_id], new ServerServiceType{tag->stream_id, ServerServiceType::CompletionQueueType::READ});
        break;
    }
    case ServerServiceType::CompletionQueueType::READ:{
        Request request_payload;
        int stream_id_ = tag->stream_id;
        if (isSuspend(stream_id_) 
        && requests_[stream_id_].setting() == ServiceRPCRequest::PULL_NEW_REQ) {
            requests_[stream_id_].payload().UnpackTo(&request_payload);
            setProcess(stream_id_);
            future_storages[stream_id_] = async(launch::async, [this, request_payload, stream_id_]() {
                Reply reply_payload;
                cb_func(&request_payload, &reply_payload, this, stream_id_);
                ServiceRPCReply reply;
                reply.set_status(endStatus(stream_id_));
                reply.mutable_payload()->PackFrom(reply_payload);
                write_reply(reply, stream_id_);
                setSuspend(stream_id_);
            });
        } else if (isProcess(stream_id_) 
        && requests_[stream_id_].setting() == ServiceRPCRequest::SET_CANCELED) {
            setPrompted(stream_id_);
        } else {
            LOG(WARNING) << "invalid operation, will not reponse to client";
        }
        streams_[stream_id_]->Read(&requests_[stream_id_], new ServerServiceType{stream_id_, ServerServiceType::CompletionQueueType::READ});
        break;
    }
    case ServerServiceType::CompletionQueueType::WRITE: {
        unique_lock<shared_mutex> lock(reply_wait_queue_mtx);
        if (!reply_wait_queue[tag->stream_id].empty()) {
            auto reply = reply_wait_queue[tag->stream_id].front();
            streams_[tag->stream_id]->Write(reply, new ServerServiceType{tag->stream_id, ServerServiceType::CompletionQueueType::WRITE});
            reply_wait_queue[tag->stream_id].pop();
        }
        break;
    }
    case ServerServiceType::CompletionQueueType::BROKEN_PIPE: {
        reusable_id.push(tag->stream_id);
        LOG(INFO) << "Client stream " << tag->stream_id << " closed";
        break;
    }
    default:
        break;
    }
}
}