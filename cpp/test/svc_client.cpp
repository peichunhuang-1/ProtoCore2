#include "core.hpp"
#include "std.pb.h"
#include <iostream>
#define BLUE "\033[1;34m"
#define RESET "\033[0m"

int main(int argc, char* argv[]) {
    if (argc < 3) {
        LOG(WARNING) << "Require at least 2 arguments: name, namespace, "
                     << "\nand remember {$namespace$name} should be unique for each node";
        return 1;
    }

    std::shared_ptr<core::NodeHandler> nh = std::make_shared<core::NodeHandler>(argv[1], argv[2]);
    nh->Init();
    core::ServiceClient<std_msgs::String, std_msgs::String> service_client = nh->serviceClient<std_msgs::String, std_msgs::String>("hello");

    std_msgs::String request;
    request.set_data("test request");
    std::future<std_msgs::String> reply;

    while (core::ok()) {
        if (!service_client.request(request, reply)) {
            LOG(INFO) << "Service server was not ready, trying again in 1 second."
                      << "\nNotice: If the request does not return properly, avoid using future.get() to get the reply,"
                      << "\nor you should handle std::future::exception.";
            std::this_thread::sleep_for(std::chrono::seconds(1));
            continue;
        }

        LOG(INFO) << BLUE << "Request sent successfully. Do you want to cancel the request? (y/n): " << RESET;
        char user_input;
        std::cin >> user_input;

        if (user_input == 'y' || user_input == 'Y') {
            LOG(INFO) << "Attempting to cancel the service request...";

            if (!service_client.cancel()) {
                LOG(INFO) << "Cancel operation failed. The service was either not requested, has done, or the server has shut down."
                          << "\nIf you are sure the request was sent (request return true), check the server's status by calling future.get(),"
                          << "\nas it may throw core::StreamCloseException if the server has been terminated.";
            } else {
                LOG(INFO) << "Service request successfully canceled.";
            }
        } else {
            LOG(INFO) << "Proceeding without canceling the service request.";
        }

        std_msgs::String receive_reply;
        try {
            receive_reply = reply.get();
            LOG(INFO) << "Service server stream status: " << service_client.ServerStatus();
            LOG(INFO) << "Received reply: " << receive_reply.data();
        } catch (const core::StreamCloseException& e) {
            LOG(INFO) << "Exception caught: " << e.what();
            LOG(INFO) << "The service server has close, reset the client and try to reconnect";
            service_client.reset(); // The service server was killed, reset to wait for a new one.
        }

        LOG(INFO) << "Completed the entire process, now ready to send a new request.";
    }

    return 0;
}
