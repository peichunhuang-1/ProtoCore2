#include "Registrar.hpp"

namespace core {
    Registrar::Registrar() {
        version = 0;
    }

    bool Registrar::find_exist_node(const string& name) {
        shared_lock<shared_mutex> lock(mtx);
        return alive_nodes.count(name);
    }

    const int Registrar::registration(const int& fixed_version, const string& name, const string& ip, const int& port) {
        NodeInfo info;
        info.set_ip(ip);
        info.set_port(port);
        info.set_node_name(name);
        info.set_code(NodeInfo::ADD);
        unique_lock<shared_mutex> lock(mtx);
        if (nodes.size() <= fixed_version) nodes.resize(fixed_version + 1000);
        nodes[fixed_version] = pair<int, NodeInfo>(-1, info);
        alive_nodes.insert(name);
        return fixed_version;
    }
    void Registrar::deregistration(const int& regist_version) {
        const int deleted_version = version ++;
        unique_lock<shared_mutex> lock(mtx);
        if (nodes.size() <= deleted_version) nodes.resize(deleted_version + 1000);
        NodeInfo info = nodes[regist_version].second;
        info.set_code(NodeInfo::DELETE);
        nodes[deleted_version] = pair<int, NodeInfo>(-1, info);
        nodes[regist_version].first = deleted_version;
        alive_nodes.erase(info.node_name());
    }


    RegistrationReply Registrar::list(const int& from, const int& to) {
        unordered_set<int> skip_version;
        RegistrationReply reply;
        reply.set_version(to);

        shared_lock<shared_mutex> lock(mtx);
        for (int i = from; i <= to; i++) {
            if (skip_version.count(i)) continue;
            if (nodes[i].first > 0) {
                skip_version.insert(nodes[i].first);
                continue;
            }
            reply.mutable_operation_cmd()->Add(NodeInfo(nodes[i].second));
        }
        return reply;
    }
    Status RegistServiceImpl::Regist(ServerContext* context, const RegistrationRequest* request,
    grpc::ServerWriter<RegistrationReply>* writer) {
        if (registrar.find_exist_node(request->info().node_name())) {
            unique_lock<mutex> lock(mtx);
            cv.wait(lock, [this, request, context](){
                return !this->registrar.find_exist_node(request->info().node_name()) || context->IsCancelled();});
            if (context->IsCancelled()) return Status::OK;
        }
        const int regist_version = registrar.registration(registrar.version++, request->info().node_name(), request->info().ip(), request->info().port());
        LOG(INFO) << "regist service version: " << regist_version << " on name: " << request->info().node_name() << "@" << request->info().ip() << ":" << request->info().port();
        RegistrationReply reply = registrar.list(0, regist_version - 1);
        writer->Write(reply);
        cv.notify_all();
        int kept_version = regist_version;
        while (core::ok()) {
            if (context->IsCancelled()) break;
            unique_lock<mutex> lock(mtx);
            cv.wait(lock);
            int cversion = registrar.version - 1;
            if (kept_version + 1 <= cversion) reply = registrar.list(kept_version + 1, cversion);
            else continue;

            if (!writer->Write(reply)) break;
            kept_version = cversion;
        }
        LOG(INFO) << "delete service: " << " on name: " << request->info().node_name() << "@" << request->info().ip() << ":" << request->info().port();
        registrar.deregistration(regist_version);
        cv.notify_all();
        return Status::OK;
    }
    void RegistServiceImpl::Healthy_Check() {
        while (core::ok()) {
            cv.notify_all();
            LOG(INFO) << "health check wake up, current version: " <<  registrar.version - 1;
            this_thread::sleep_for(chrono::seconds(healthy_check_sec));
        }
    }
    void RunServer(string server_address) {
        LOG(INFO) << "Server network address set to: " << server_address;
        RegistServiceImpl service;
        thread heath_check_thread(bind(&RegistServiceImpl::Healthy_Check, &service));
        heath_check_thread.detach();
        grpc::EnableDefaultHealthCheckService(true);
        grpc::reflection::InitProtoReflectionServerBuilderPlugin();
        ServerBuilder builder;
        builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
        builder.RegisterService(&service);
        std::unique_ptr<Server> server(builder.BuildAndStart());
        server->Wait();
        google::FlushLogFiles(google::GLOG_INFO);
    }
}

int main(int argc, char* argv[]) {
    core_exception::catcher_init();
    const char* log_dir_env = getenv("CORE_LOG_DIR");
    if (log_dir_env) {
        FLAGS_log_dir = std::string(getenv("CORE_LOG_DIR"));
    } else {
        FLAGS_logtostderr = 1; 
        FLAGS_colorlogtostderr = 1;
    }
    google::InitGoogleLogging("NodeCore");
    core::RunServer(std::string(getenv("CORE_MASTER_ADDR") ? getenv("CORE_MASTER_ADDR") : DEFAULT_CORE_MASTER_ADR));
    return 0;
}