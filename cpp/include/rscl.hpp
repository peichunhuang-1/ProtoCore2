#ifndef RSCL_HPP
#define RSCL_HPP
#include <google/protobuf/message.h>
#include "rscl.grpc.pb.h"
#include "NodeRegist.hpp"
#include "serialization.hpp"
#include "TCPServer.hpp"
#include "TCPClient.hpp"
#include "Publisher.hpp"
#include "Subscriber.hpp"
#include "ServiceServer.hpp"
#include "ServiceClient.hpp"
#include "ParamRPC.hpp"
#include "TransformTree.hpp"
#include <mutex>
#include <future>
#include <memory>

namespace core {
    using namespace rscl;
    using namespace std;
    using namespace grpc;
    class NodeRegister;
    class NodeConnectionClient;
    class NodeConnectionClientClub;
    class NodeConnectionServerImpl;
    struct client_info {
        public:
        string                              ip = "";
        int                                 port = 0;
        future<bool>                        connected;
    };
    class NodeHandler: public enable_shared_from_this<NodeHandler> {
        public:
        NodeHandler(const string& name_, const string& namespace_);
        void                                    Init();
        template<class msg_t>
        Subscriber                              subscribe(const string& topic, void (*cb)(const msg_t*));
        template<class msg_t>
        Subscriber                              subscribe(const string& topic, function<void(const msg_t*)> cb);
        template<class msg_t>
        Publisher<msg_t>                        advertise(const string& topic);
        void                                    spinOnce();
        template<typename Request, typename Reply>
        ServiceClient<Request, Reply>           serviceClient(const string& service);
        template<typename Request, typename Reply>
        ServiceServer<Request, Reply>           serviceServer(const string& service, void(*)(const Request*, Reply*, ServiceServer<Request, Reply>*, const int&));

        TransformBroadcaster                    tfBroadcaster();
        StaticTransformBroadcaster              tfStaticBroadcaster();
        TransformListener                       tfListener();

        template<typename param_t>
        void                                    setParameter(const string& name, const param_t val);
        template<typename param_t>
        void                                    declareParameter(const string& name, const param_t val, bool is_const = false);
        template<typename param_t>
        bool                                    getParameter(const string& name, param_t &val);
        template<typename param_t>
        bool                                    getDynamicParameter(const string& name, void(*)(const param_t));
        
        template<typename param_t>
        ParameterStatus                         setRemoteParameter(const string& name, const param_t val, const string& namespace_);
        template<typename param_t>
        ParameterStatus                         getRemoteParameter(const string& name, param_t &val, const string& namespace_);
        template<typename param_t>
        ParameterStatus                         getRemoteDynamicParameter(const string& name, void(*)(const param_t), const string& namespace_);


        private:
        void                                    regist_node(const string& node, const string& ip, const int& port);
        template<class msg_t>
        Subscriber                              subscribe_impl(const string& topic, function<void(const msg_t*)> cb);
        void                                    delete_node(const string& node);
        const string                            this_node_name();

        bool                                    find_wait_published_topic(const string& topic, const string& url);
        void                                    add_published_topic(const string& topic, const string& url);
        void                                    add_subscribed_topic(const string& topic, const string& url);
        client_info                             add_tcp_client(const string& node, const string& topic, const string& ip, const int& port);

        bool                                    find_wait_served_service(const string& service);
        bool                                    find_serving_service(const string& service);
        future<string>                          add_served_service(const string& service);
        future<string>                          reset_served_service(const string& service);
        void                                    add_serving_service(const string& service);
        bool                                    add_rpc_service_client(const string& node, const string& service, const string& ip, const int& port);

        bool                                    accept_topic_publish(const string& topic, const string& ip, const int& port);
        bool                                    accept_service_client(const string& service);

        const string                            name;
        string                                  this_node_connection_rpc_ip;
        int                                     this_node_connection_rpc_port;
        int                                     this_node_tcp_port;

        shared_ptr<NodeConnectionServerImpl>    connection_rpc_service;
        unique_ptr<grpc::Server>                connection_rpc_server;
        shared_ptr<NodeConnectionClientClub>    connection_rpc_clients;
        shared_ptr<NodeRegist>                  node_register;
        shared_ptr<TCPClient>                   tcp_topic_clients;
        shared_ptr<TCPServer>                   tcp_topic_server;
        shared_ptr<ParamRPCClientClub>          param_clients;
        shared_ptr<ParamRPCServerImpl>          param_server;

        using tf_publisher = shared_ptr<Publisher<std_msgs::TransformD>>;
        tf_publisher                            tf_pub;
        tf_publisher                            static_tf_pub;

        unordered_map<string, string>           topics;
        shared_mutex                            topics_mtx;

        unordered_map<string, promise<string>>  services;
        shared_mutex                            services_mtx;

        friend class                            core::NodeRegist;
        friend class                            core::NodeConnectionClient;
        friend class                            core::NodeConnectionClientClub;
        friend class                            core::NodeConnectionServerImpl;
        template<typename T>
        friend class                            core::Publisher;
        friend class                            core::Subscriber;
        template<typename Request, typename Reply>
        friend class                            core::ServiceServer;
        template<typename Request, typename Reply>
        friend class                            core::ServiceClient;
    };
    
    class NodeConnectionServerImpl final :public NodeConnection::Service {
        public:
        NodeConnectionServerImpl(shared_ptr<core::NodeHandler> nh);
        Status                                  TopicConnection(ServerContext* context, 
                                                const ConnectionRequest*request, ConnectionReply* reply);
        Status                                  ServiceConnection(ServerContext* context, 
                                                const ConnectionRequest*request, ConnectionReply* reply);
        Status                                  TreeConnection(ServerContext* context, 
                                                const ConnectionRequest*request, ConnectionReply* reply);
        void                                    notify_all();
        private:
        condition_variable                      cv;
        mutex                                   mtx;
        shared_ptr<core::NodeHandler>           nh_;
    };

    class NodeConnectionClient final {
        public:
        NodeConnectionClient(shared_ptr<Channel> channel, shared_ptr<core::NodeHandler> nh);
        ~NodeConnectionClient();
        void                                    pull_subscribe_request(const ConnectionRequest& request);
        void                                    pull_serving_service_request(const ConnectionRequest& request);
        void                                    pull_possess_tree_request(const ConnectionRequest& request);
        private:
        unique_ptr<NodeConnection::Stub>        stub_;
        shared_ptr<core::NodeHandler>           nh_;
        queue<thread>                           write_queue;
        shared_mutex                            write_queue_mtx;
    };

    class NodeConnectionClientClub final {
        public:
        NodeConnectionClientClub(shared_ptr<core::NodeHandler> nh) ;
        void                                    add_client(const string& node, const string& rpc_srv_addr);
        void                                    delete_client(const string& node);
        void                                    pull_subscribe_request(const string& topic, const string& ip, const int& port, const string& type_url);
        void                                    pull_serving_service_request(const string& service, const string& ip, const int& port);
        void                                    pull_possess_tree_request(const string& tree, const string& ip, const int& port);
        
        private:
        using ClientClub = unordered_map<string, unique_ptr<NodeConnectionClient>>;
        shared_ptr<core::NodeHandler>           nh_;
        ClientClub                              clients;
        shared_mutex                            mtx;
        vector<const ConnectionRequest>         topic_requests;
        vector<const ConnectionRequest>         service_requests;
        vector<const ConnectionRequest>         param_requests;
        vector<const ConnectionRequest>         tree_requests;
    };
    template<class msg_t>
    Subscriber NodeHandler::subscribe(const string& topic, void (*cb)(const msg_t*)) {
        return subscribe_impl(topic, function<void(const msg_t*)>(cb));
    }

    template<class msg_t>
    Subscriber NodeHandler::subscribe(const string& topic, function<void(const msg_t*)> cb) {
        return subscribe_impl(topic, cb);
    }

    template<class msg_t>
    Subscriber NodeHandler::subscribe_impl(const string& topic, function<void(const msg_t*)> cb) {
        string url = get_typeurl<msg_t>();
        add_subscribed_topic(topic, url);
        connection_rpc_clients->pull_subscribe_request(topic, this_node_connection_rpc_ip, this_node_tcp_port, url);
        if (!tcp_topic_server->decoders.count(topic)) {
            tcp_topic_server->decoders[topic] = new SpecifiedDecoder<msg_t>();
        }
        static_cast<SpecifiedDecoder<msg_t>*>(tcp_topic_server->decoders[topic])->add_callback(cb);
        return Subscriber(topic, shared_from_this());
    }
    template<class msg_t>
    Publisher<msg_t> NodeHandler::advertise(const string& topic) {
        string url = get_typeurl<msg_t>();
        add_published_topic(topic, url);
        connection_rpc_service->notify_all();
        return Publisher<msg_t>(topic, shared_from_this());
    }
    template<typename T>
    Publisher<T>::Publisher(const string& topic, shared_ptr<core::NodeHandler> nh) 
    : pub_topic(topic), nh_(nh){}
    template<typename T>
    void Publisher<T>::publish(const T& msg, bool cache) {
        string buffer = core::serialize(msg);
        if (cache) {
            if (!nh_->tcp_topic_clients->write_to_cache(pub_topic, buffer) ) return;
        }
        nh_->tcp_topic_clients->write_to_socket(pub_topic, buffer);
    }

    template<typename Request, typename Reply>
    ServiceServer<Request, Reply> NodeHandler::serviceServer(const string& service, void(*cb)(const Request*, Reply*, ServiceServer<Request, Reply>*, const int&)) {
        if ( find_serving_service(service) ) return ServiceServer<Request, Reply>();
        add_serving_service(service);
        int service_server_port;
        return ServiceServer<Request, Reply>(service, this_node_connection_rpc_ip, service_server_port, cb, shared_from_this());
    }
    template<typename Request, typename Reply>
    ServiceClient<Request, Reply> NodeHandler::serviceClient(const string& service) {
        future<string> srv_addr = add_served_service(service);
        connection_rpc_service->notify_all();
        return ServiceClient<Request, Reply>(service, ref(srv_addr), shared_from_this());
    }

    template<typename Request, typename Reply>
    ServiceServer<Request, Reply>::ServiceServer(const string& service, const string& ip, int& rpc_port, FunctionType cb, shared_ptr<core::NodeHandler> nh): 
    service_name(service), cb_func(cb), nh_(nh) {
        valid_ = true;
        stream_id = 0;
        ServerBuilder builder;
        builder.AddListeningPort(ip+":0", grpc::InsecureServerCredentials(), &rpc_port);
        cq_ = builder.AddCompletionQueue();
        builder.RegisterService(this);
        server_ = builder.BuildAndStart();
        for (int i = 0; i < 10; i++){
            int id = generate_stream_id();
            CreateListenPort(id);
        }
        handle_event_thread = thread([this]() {
            while(core::ok()) {
                void* got_tag = nullptr;
                bool ok = false;
                if (!cq_->Next(&got_tag, &ok)) {
                    LOG(ERROR) << "Server stream closed. Quitting";
                    break;
                }
                auto tag = static_cast<ServerServiceType*>(got_tag);
                if (ok) handle_event(tag);
                delete tag;
            }
        });
        nh_->connection_rpc_clients->pull_serving_service_request(service, ip, rpc_port);
    }

    template<typename Request, typename Reply>
    void ServiceClient<Request, Reply>::reset() {
        unique_lock<shared_mutex> lock(usr_operation_mtx);
        wait_server_addr = move(nh_->reset_served_service(service_name));
        wait_server_notify_thread.join();
        wait_server_notify_thread = thread([this]() mutable {
            const string srv_addr = wait_server_addr.get();
            create_connection(srv_addr);
        });
        LOG(INFO) << "reset service client stream";
    }

    template<typename param_t>
    void NodeHandler::setParameter(const string& name, const param_t val) {
        param_server->set(name, val);
    }
    template<typename param_t>
    void NodeHandler::declareParameter(const string& name, const param_t val, bool is_const) {
        param_server->declare(name, val, is_const);
    }
    template<typename param_t>
    bool NodeHandler::getParameter(const string& name, param_t &val) {
        return param_server->get(name, val);
    }
    template<typename param_t>
    bool NodeHandler::getDynamicParameter(const string& name, void(*cb)(const param_t)) {
        param_t val;
        return param_server->get(name, val, cb);
    }
    
    template<typename param_t>
    ParameterStatus NodeHandler::setRemoteParameter(const string& name, const param_t val, const string& namespace_) {
        return param_clients->setRemoteRequest(name, val, namespace_);
    }
    template<typename param_t>
    ParameterStatus NodeHandler::getRemoteParameter(const string& name, param_t &val, const string& namespace_) {
        return param_clients->getRemoteRequest(name, val, namespace_);
    }
    template<typename param_t>
    ParameterStatus NodeHandler::getRemoteDynamicParameter(const string& name, void(*cb)(const param_t), const string& namespace_) {
        return param_clients->getRemoteRequest(name, cb, namespace_);
    }

}

#endif