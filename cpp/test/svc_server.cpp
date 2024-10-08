#include "core.hpp"
#include "std.pb.h"

void service_cb(const std_msgs::String* request, std_msgs::String* reply, 
core::ServiceServer<std_msgs::String, std_msgs::String>*service_server, const int& stream_id) {
    // the call back can be blocked function, but please check if the service is aborted internally 
    // if the client died, the status would switch to aborted automatically.
    LOG(INFO) << "receive request: " << request->data();
    int count = 0;
    while(core::ok() && !service_server->isAborted(stream_id)) {
        if (service_server->isPrompted(stream_id)) {
            // if client send cancel, the service stream status would be prompted
            LOG(INFO) << "refuse to abort service";
            // do nothing is ok, of course you can ignore that,
            // or you can set abort, success, or failed, like this:
            // service_server->setAborted(stream_id);
            // break; // remember break !!
        }
        sleep(1);
        if (count ++ > 10) break; 
        // random simple break condition, just do not make the function blocked forever, 
        // that will cause the service server block when client was killed. 
    }
    service_server->setSuccess(stream_id); 
    // if you don't set the final status (aborted, success, failed), it is default to set abort.
    reply->set_data("reply test"); 
    // the reply message will send once you were out of this function (of course the client should be active, if not, nothing happen).
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        LOG(WARNING) << "Require at least 2 argument: name, namespace, "
        "\nand remember {$namespace$name} should be unique for each node";
        return 1;
    }
    std::shared_ptr<core::NodeHandler> nh = std::make_shared<core::NodeHandler>(argv[1], argv[2]);
    nh->Init();
    core::ServiceServer<std_msgs::String, std_msgs::String> service_server = nh->serviceServer<std_msgs::String, std_msgs::String>("hello", service_cb);
    while(core::ok()) {
        std::this_thread::sleep_for(std::chrono::seconds(1)); // just for not returning main.
    }
    return 0;
}