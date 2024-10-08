#include "rscl.hpp"
#include "NodeRegist.hpp"
namespace core {
NodeHandler::NodeHandler(const string& name_, const string& namespace_) :
name(namespace_ + "/" + name_) {
    core_exception::catcher_init();
    const char* log_dir_env = getenv("CORE_LOG_DIR");
    if (log_dir_env) {
        FLAGS_log_dir = std::string(log_dir_env);
    } else {
        FLAGS_logtostderr = 1; 
        FLAGS_colorlogtostderr = 1;
    }
    google::InitGoogleLogging(name.c_str());
    const char* local_ip = getenv("CORE_LOCAL_IP");
    if (local_ip) {
        this_node_connection_rpc_ip = std::string(local_ip);
    } else {
        this_node_connection_rpc_ip = "127.0.0.1";
    }
}

void NodeHandler::Init() {
    tcp_topic_clients = make_shared<TCPClient>();
    tcp_topic_server = make_shared<TCPServer>(this_node_connection_rpc_ip);

    param_server = make_shared<ParamRPCServerImpl>();
    param_clients = make_shared<ParamRPCClientClub>();

    this_node_tcp_port = tcp_topic_server->init_tcp_srv();
    connection_rpc_service = make_shared<NodeConnectionServerImpl>(shared_from_this());
    ServerBuilder builder;
    int rpc_port_;
    builder.AddListeningPort(this_node_connection_rpc_ip+":0", grpc::InsecureServerCredentials(), &rpc_port_);
    builder.RegisterService(connection_rpc_service.get());
    builder.RegisterService(param_server.get());
    connection_rpc_server = builder.BuildAndStart();
    this_node_connection_rpc_port = rpc_port_;

    connection_rpc_clients = make_shared<NodeConnectionClientClub>(shared_from_this());
    node_register = make_shared<NodeRegist>(name, shared_from_this());
}

void NodeHandler::spinOnce() {
    tcp_topic_server->event_handler();
}

void NodeHandler::regist_node(const string& node, const string& ip, const int& port) {
    LOG(INFO) << "connect to connection server on node: " << node << "@" << ip << ":" << port;
    connection_rpc_clients->add_client(node, ip + ":" + to_string(port));
    param_clients->add_client(node, ip + ":" + to_string(port));
}

void NodeHandler::delete_node(const string& node) {
    LOG(INFO) << "delete connection server between this and node: " << node;
    connection_rpc_clients->delete_client(node);
    param_clients->delete_client(node);
}

const string NodeHandler::this_node_name() {return name;}

bool NodeHandler::find_wait_published_topic(const string& topic, const string& url) {
    shared_lock<shared_mutex> lock(topics_mtx);
    const string token = "p" + topic;
    return topics.count(token) && (topics[token] == url);
}

void NodeHandler::add_published_topic(const string& topic, const string& url) {
    unique_lock<shared_mutex> lock(topics_mtx);
    topics["p" + topic] = url;
}

void NodeHandler::add_subscribed_topic(const string& topic, const string& url) {
    unique_lock<shared_mutex> lock(topics_mtx);
    topics["s"+topic] = url;
}

client_info NodeHandler::add_tcp_client(const string& node, const string& topic, const string& ip, const int& port) {
    /* 
    create a tcp client connect to input server,
    map client object to that topic in tcp clients.
    */
    client_info info = tcp_topic_clients->add_client(topic, ip, port);
    return info;
}

bool NodeHandler::find_wait_served_service(const string& service) {
    shared_lock<shared_mutex> lock(services_mtx);
    const string token = "d" + service;
    return services.count(token);
}
future<string> NodeHandler::add_served_service(const string& service) {
    unique_lock<shared_mutex> lock(services_mtx);
    const string token = "d" + service;
    if (services.count(token)) {
        core::setAbort();
        LOG(ERROR) << "dulicated service client";
        exit(-1);
    }
    services[token] = promise<string>();
    return services["d" + service].get_future();
}

future<string> NodeHandler::reset_served_service(const string& service) {
    unique_lock<shared_mutex> lock(services_mtx);
    const string token = "d" + service;
    services[token] = promise<string>();
    return services["d" + service].get_future();
}

void NodeHandler::add_serving_service(const string& service) {
    unique_lock<shared_mutex> lock(services_mtx);
    services["g" + service] = promise<string>();
}
bool NodeHandler::find_serving_service(const string& service) {
    shared_lock<shared_mutex> lock(services_mtx);
    const string token = "g" + service;
    return services.count(token);
}
bool NodeHandler::add_rpc_service_client(const string& node, const string& service, const string& ip, const int& port) {
    /* 
    create a rpc client connect to input server,
    map client object to that service in rpc clients.
    */
    unique_lock<shared_mutex> lock(services_mtx);
    try {
        services["d" + service].set_value(ip + ":" + to_string(port));
        return true;
    } catch (const std::future_error& e) {
        return false;
    } catch (const std::exception& e) {
        services["d" + service].set_exception(current_exception());
        return false;
    }
}

bool NodeHandler::accept_topic_publish(const string& topic, const string& ip, const int& port) {
    /* 
    set topic slot state used by client@ip:port
    */
    tcp_topic_server->accept_client(topic, ip, port);
    return true;
}
bool NodeHandler::accept_service_client(const string& service) {
    /* 
    not yet finished, but working
    */
    return true;
}

NodeConnectionServerImpl::NodeConnectionServerImpl(shared_ptr<core::NodeHandler> nh): nh_(nh) {}

Status NodeConnectionServerImpl::TopicConnection(grpc::ServerContext* context, 
const ConnectionRequest*request, ConnectionReply* reply) {
    const string node = request->node();
    const string topic = request->object();
    const string ip = request->ip();
    const int port = request->port();
    const string type_url = request->url();
    while (core::ok() && !context->IsCancelled()) {
        if (nh_->find_wait_published_topic(topic, type_url)) {
            auto client_info = nh_->add_tcp_client(node, topic, ip, port);
            bool connected = client_info.connected.get();
            if (! connected) {
                LOG(ERROR) << "failed to establish connection on topic: " << topic;
                return Status::OK;
            }
            reply->set_object(topic);
            reply->set_ip(client_info.ip);
            reply->set_port(client_info.port);
            reply->set_url(type_url);
            break;
        }
        unique_lock<mutex> lock(mtx);
        cv.wait(lock, [&]() {
            return nh_->find_wait_published_topic(topic, type_url) || !core::ok();
        });
    }
    LOG(INFO) << "create connection on topic: " << topic;
    return Status::OK;
}

Status NodeConnectionServerImpl::ServiceConnection(grpc::ServerContext* context, 
const ConnectionRequest*request, ConnectionReply* reply) {
    const string node = request->node();
    const string service = request->object();
    const string ip = request->ip();
    const int port = request->port();
    if (nh_->find_serving_service(service)) {
        core::setAbort();
        LOG(ERROR) << "service name conflict with other service server, exit...";
        exit(-1);
    }
    while (core::ok() && !context->IsCancelled()) {
        if (nh_->find_wait_served_service(service)) {
            if (!nh_->add_rpc_service_client(node, service, ip, port)) {
                LOG(ERROR) << "temporary failed to establish connection on service: " << service;
                this_thread::sleep_for(chrono::seconds(1));
                continue;
            }
            reply->set_object(service);
            break;
        }
        unique_lock<mutex> lock(mtx);
        cv.wait(lock, [&]() {
            return nh_->find_wait_served_service(service) || !core::ok();
        });
    }
    LOG(INFO) << "create connection on service: " << service;
    return Status::OK;
}

Status NodeConnectionServerImpl::TreeConnection(grpc::ServerContext* context, 
const ConnectionRequest*request, ConnectionReply* reply) {
    return Status::OK;
}

void NodeConnectionServerImpl::notify_all() {
    cv.notify_all();
}

NodeConnectionClient::NodeConnectionClient(shared_ptr<Channel> channel, shared_ptr<core::NodeHandler> nh): 
nh_(nh), stub_(NodeConnection::NewStub(channel)) {}

NodeConnectionClient::~NodeConnectionClient() {
    unique_lock<shared_mutex> lock(write_queue_mtx);
    while (!write_queue.empty()) {
        write_queue.front().join();
        write_queue.pop();
    }
}

void NodeConnectionClient::pull_subscribe_request(const ConnectionRequest& request) {
    unique_lock<shared_mutex> lock(write_queue_mtx);
    write_queue.push(
        thread( [this, request](){
        ClientContext context;
        ConnectionReply reply;
        Status status = stub_->TopicConnection(&context, request, &reply);
        if (status.ok()) {
            if (this->nh_->accept_topic_publish(reply.object(), reply.ip(), reply.port()))
                LOG(INFO) << "accept topic pubilsher on topic: " << reply.object() << "@" << reply.ip() << ":" << reply.port();
        } else {
            LOG(ERROR) << "failed to accept publisher on topic: " << reply.object() << "@" << reply.ip() << ":" << reply.port();
        }})
    );
}
void NodeConnectionClient::pull_serving_service_request(const ConnectionRequest& request) {
    unique_lock<shared_mutex> lock(write_queue_mtx);
    write_queue.push(
        thread( [this, request](){
        ClientContext context;
        ConnectionReply reply;
        Status status = stub_->ServiceConnection(&context, request, &reply);
        if (status.ok()) {
            if (this->nh_->accept_service_client(reply.object()))
                LOG(INFO) << "accept service client on service: " << reply.object();
        } else {
            LOG(ERROR) << "failed to accept service client on service: " << reply.object();
        }})
    );
}

void NodeConnectionClient::pull_possess_tree_request(const ConnectionRequest& request) {

}

NodeConnectionClientClub::NodeConnectionClientClub(shared_ptr<core::NodeHandler> nh) : nh_(nh) {}

void NodeConnectionClientClub::add_client(const string& node, const string& rpc_srv_addr) {
    unique_lock<shared_mutex> lock(mtx);
    clients[node] = make_unique<NodeConnectionClient>( CreateChannel(rpc_srv_addr, grpc::InsecureChannelCredentials()), nh_ );
    for (auto &rq: topic_requests) clients[node]->pull_subscribe_request(rq);
    for (auto &rq: service_requests) clients[node]->pull_serving_service_request(rq);
    for (auto &rq: tree_requests) clients[node]->pull_possess_tree_request(rq);
}

void NodeConnectionClientClub::delete_client(const string& node) {
    unique_lock<shared_mutex> lock(mtx);
    clients.erase(node);
}

void NodeConnectionClientClub::pull_subscribe_request(const string& topic, const string& ip, const int& port, const string& type_url) {
    ConnectionRequest request;
    request.set_object(topic);
    request.set_ip(ip);
    request.set_port(port);
    request.set_url(type_url);
    request.set_node(nh_->this_node_name());
    unique_lock<shared_mutex> lock(mtx);
    topic_requests.push_back(request);
    for (auto &client: clients) {
        client.second->pull_subscribe_request(request);
    }
}

void NodeConnectionClientClub::pull_serving_service_request(const string& service, const string& ip, const int& port) {
    ConnectionRequest request;
    request.set_object(service);
    request.set_ip(ip);
    request.set_port(port);
    request.set_node(nh_->this_node_name());
    unique_lock<shared_mutex> lock(mtx);
    service_requests.push_back(request);
    for (auto &client: clients) {
        client.second->pull_serving_service_request(request);
    }
}

void NodeConnectionClientClub::pull_possess_tree_request(const string& tree, const string& ip, const int& port) {

}

}