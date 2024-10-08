#include "core.hpp"
#include "std.pb.h"

int main(int argc, char* argv[]) {
    if (argc < 3) {
        LOG(WARNING) << "Require at least 2 argument: name, namespace, "
        "\nand remember {$namespace$name} should be unique for each node";
        return 1;
    }

    std::shared_ptr<core::NodeHandler> nh = std::make_shared<core::NodeHandler>(argv[1], argv[2]);
    nh->Init();
    core::ServiceClient<std_msgs::String, std_msgs::String> service_client = nh->serviceClient<std_msgs::String, std_msgs::String>("hello");
    std_msgs::String request;
    request.set_data("test request");
    std::future<std_msgs::String> reply;
    while(core::ok()) {
        if ( !service_client.request(request, reply) ) {
            LOG(INFO) << "service server was not ready, try again 1 second later,"
            "\nnotice if the request was not return properly, do not use future.get() to get the reply,"
            "\nor you should catch std::future::exception";
            std::this_thread::sleep_for(std::chrono::seconds(1));
            continue;
        }

        LOG(INFO) << "try to cancel service request 1 second later...";
        std::this_thread::sleep_for(std::chrono::seconds(1));

        if ( !service_client.cancel() ) {
            LOG(INFO) << "cancel operation failed, service was not requested, or server has shutdown, "
            "\nsince you are sure the request has sent here, so the expected way to check if the server was alive,"
            "\n is to call future.get(), if the server was killed, the reply may get core::StreamCloseException";
        }

        std_msgs::String receive_reply;
        try {
            receive_reply = reply.get();
            LOG(INFO) << "service server stream status: " << service_client.ServerStatus();
            LOG(INFO) << "receive reply: " << receive_reply.data();
        } catch (const core::StreamCloseException& e) {
            LOG(INFO) << e.what();
            service_client.reset(); // the service server was killed, reset to wait the new one.
        }

        LOG(INFO) << "go through a whole process, now we can pull a new request";
    }
}