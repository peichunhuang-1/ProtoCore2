#ifndef PARAM_RPC_HPP
#define PARAM_RPC_HPP
#include "param.grpc.pb.h"
#include <thread>
#include <queue>
#include "common.hpp"
#include <grpcpp/ext/proto_server_reflection_plugin.h>
#include <grpcpp/grpcpp.h>
#include <grpcpp/health_check_service_interface.h>
#include <glog/logging.h>

namespace core {
using namespace param_rpc;
using namespace std;
using namespace grpc;
class ParameterBase {
    public:
    virtual void                            setRemoteValue(const google::protobuf::Any& val) = 0;
    virtual google::protobuf::Any           getRemoteValue() = 0;
};

template<class T>
class Parameter : public ParameterBase {
    public:
    Parameter(T val_) : val(val_)           {}
    void                                    set(T val_) ;
    T                                       get() ;
    virtual void                            setRemoteValue(const google::protobuf::Any& val_) override;
    virtual google::protobuf::Any           getRemoteValue() override ;

    private:
    T                                       val;
    shared_mutex                            mtx;
};

template<class T>
void Parameter<T>::set(T val_) {
    unique_lock<shared_mutex> lock(mtx);
    val = val_;
}
template<class T>
T Parameter<T>::get() {
    shared_lock<shared_mutex> lock(mtx);
    return val;
}
template<class T>
void Parameter<T>::setRemoteValue(const google::protobuf::Any& val_) {
    unique_lock<shared_mutex> lock(mtx);
    val_.UnpackTo(&val);
}
template<class T>
google::protobuf::Any Parameter<T>::getRemoteValue() {
    shared_lock<shared_mutex> lock(mtx);
    google::protobuf::Any val_;
    return val_.PackFrom(val), val_;
}

class ParamRPCServerImpl final :public ParamRPC::Service {
    public:
    ParamRPCServerImpl()                  = default;
    Status                                  getRemoteParam(ServerContext* context, 
                                            const getParamRPCRequest*request, ServerWriter<getParamRPCReply>* writer);
    Status                                  setRemoteParam(ServerContext* context, 
                                            const setParamRPCRequest*request, setParamRPCReply* reply);
    template<class T>
    void                                    declare(const string& name, T val, bool is_const = false);
    template<class T>                       
    void                                    set(const string& name, T val);
    template<class T>                       
    bool                                    get(const string& name, T& val, void(*cb)(const T) = nullptr);

    private:
    unordered_map<string, ParameterBase*>   params; // param_name, val
    unordered_map<string, atomic<int>>      params_version;
    shared_mutex                            params_mtx;
    unordered_set<string>                   is_const_params;
    shared_mutex                            is_const_params_mtx;

    mutex                                   mtx;
    condition_variable                      cv;

    bool                                    find_param(const string& name);
    bool                                    is_const(const string& name);
};

template<class T>
void ParamRPCServerImpl::declare(const string& name, T val, bool is_const) {
    if (find_param(name)) {
        LOG(ERROR) << "parameter: " << name << " has declared";
        return;
    }
    unique_lock<shared_mutex> lock(params_mtx);
    params[name] = static_cast<ParameterBase*> (new Parameter<T>(val));
    params_version[name] = 1;
    if (is_const) {
        unique_lock<shared_mutex> lock(is_const_params_mtx);
        is_const_params.insert(name);
    }
    cv.notify_all();
}

template<class T>                       
void ParamRPCServerImpl::set(const string& name, T val) {
    if (!find_param(name)) {
        LOG(WARNING) << "parameter: " << name << " was not declared";
        return;
    }
    if (is_const(name)) {
        LOG(WARNING) << "parameter: " << name << " is a constant";
        return;
    }
    unique_lock<shared_mutex> lock(params_mtx);
    static_cast<Parameter<T>*>(params[name])->set(val);
    params_version[name] ++;
    cv.notify_all();
}

template<class T>                       
bool ParamRPCServerImpl::get(const string& name, T& val, void(*cb)(const T)) {
    if (!cb) {
        if (find_param(name)) {
            shared_lock<shared_mutex> lock(params_mtx);
            val = static_cast<Parameter<T>*>(params[name])->get();
            return true;
        }
        return false;
    }
    thread( [this, cb, name] () {
        int version = 0;
        while (core::ok()) {
            T param;
            if (get(name, param)) {
                version = params_version[name].load();
                cb(param);
            }
            unique_lock<mutex> lock(mtx);
            cv.wait(lock, [&]() {
                return (find_param(name) && version != params_version[name].load()) || !core::ok();
            });
        }
    }).detach();
    return true;
}

class ParamSlot {
    public:
    ParamSlot()                             {data.set_status(ParameterStatus::TMEP_NOT_AVALIABLE);}
    virtual void                            get(getParamRPCReply& reply);
    virtual void                            set(const getParamRPCReply& reply);
    getParamRPCReply                        data;
};

class DynamicParamSlot : public ParamSlot {
    public:
    using CallBackFunc = function<void(const getParamRPCReply)>;
    DynamicParamSlot(CallBackFunc cb_) : cb(cb_), ParamSlot() {}
    void                                    set(const getParamRPCReply& reply) override;
    void                                    get(getParamRPCReply& reply) override;
    private:
    CallBackFunc                            cb;
};

class ParamRPCClient {
    public:
    ParamRPCClient(shared_ptr<Channel> channel);
    ~ParamRPCClient();
    setParamRPCReply                        setRemoteParam(const setParamRPCRequest& request);
    void                                    getRemoteParam(const getParamRPCRequest& request, getParamRPCReply& reply, function<void(const getParamRPCReply)> cb = nullptr);
    
    private:
    using ReceivedSet = unordered_map<string, ParamSlot*>;
    unique_ptr<ParamRPC::Stub>              stub_;
    bool                                    find_param(const string& name);
    queue<thread>                           write_thread;
    shared_mutex                            write_thread_mtx;
    ReceivedSet                             get_replies;
    shared_mutex                            get_replies_mtx;
};

class ParamRPCClientClub final {
    public:
    using CallBackFunc = function<void(const getParamRPCReply)>;
    using ClientClub = unordered_map<string, unique_ptr<ParamRPCClient>>;
    struct getRequestPack {
        public:
        getRequestPack(const getParamRPCRequest req_, CallBackFunc cb_ = nullptr) : req(req_), cb(cb_) {}
        CallBackFunc                        cb;
        const getParamRPCRequest            req;
    };

    using getRequestQueue = unordered_map<string, queue<getRequestPack>>;

    ParamRPCClientClub();
    void                                    add_client(const string& namespace_, const string& srv_addr);
    void                                    delete_client(const string& namespace_);
    template<class T>
    ParameterStatus                         setRemoteRequest(const string& name, const T& request, const string& namespace_);
    template<class T>
    ParameterStatus                         getRemoteRequest(const string& name, T& reply, const string& namespace_);
    template<class T>
    ParameterStatus                         getRemoteRequest(const string& name, void(*cb)(const T), const string& namespace_);

    private:
    ClientClub                              clients;
    shared_mutex                            mtx;
    getRequestQueue                         get_requests_queue;       
};

template<class T>
ParameterStatus ParamRPCClientClub::setRemoteRequest(const string& name, const T& request, const string& namespace_) {
    unique_lock<shared_mutex> lock(mtx);
    if (!clients.count(namespace_)) return ParameterStatus::TMEP_NOT_AVALIABLE;
    setParamRPCRequest request_;
    request_.set_name(name);
    request_.mutable_payload()->PackFrom(request);
    return clients[namespace_]->setRemoteParam(request_).status();
}

template<class T>
ParameterStatus ParamRPCClientClub::getRemoteRequest(const string& name, T& reply, const string& namespace_) {
    unique_lock<shared_mutex> lock(mtx);
    getParamRPCRequest request_;
    request_.set_name(name);
    if (!clients.count(namespace_)) {
        return ParameterStatus::TMEP_NOT_AVALIABLE;
    }
    getParamRPCReply reply_;
    clients[namespace_]->getRemoteParam(request_, reply_);
    reply_.mutable_payload()->UnpackTo(&reply);
    return reply_.status();
}

template<class T>
ParameterStatus ParamRPCClientClub::getRemoteRequest(const string& name, void(*cb)(const T), const string& namespace_) {
    unique_lock<shared_mutex> lock(mtx);
    getParamRPCRequest request_;
    request_.set_name(name);
    auto cb_wrapper = [cb](const getParamRPCReply reply_) -> void {
        T reply;
        reply_.payload().UnpackTo(&reply);
        cb(reply);
    };
    get_requests_queue[namespace_].push(getRequestPack(request_, cb_wrapper));

    if (!clients.count(namespace_)) {
        return ParameterStatus::TMEP_NOT_AVALIABLE;
    }
    getParamRPCReply reply_;
    clients[namespace_]->getRemoteParam(request_, reply_, cb_wrapper);
    return reply_.status();
}

}

#endif