#include "core.hpp"
#include "std.pb.h"
#include <iostream>

// Service callback function that handles requests and cancel prompts
void service_cb(const std_msgs::String* request, std_msgs::String* reply,
                core::ServiceServer<std_msgs::String, std_msgs::String>* service_server, const int& stream_id) {
    LOG(INFO) << "Received request: " << request->data();
    int count = 0;

    // Main loop to handle service request until aborted or completed
    while (core::ok() && !service_server->isAborted(stream_id)) { 
        // Always check if the server has been aborted at regular intervals
        if (service_server->isPrompted(stream_id)) {
            // If the client sends a cancel request, prompt the user to decide whether to accept or refuse
            LOG(INFO) << "Client requested to cancel the service. Do you want to accept the cancellation? (y/n): ";
            char user_input;
            std::cin >> user_input;

            if (user_input == 'y' || user_input == 'Y') {
                LOG(INFO) << "Cancellation accepted. Aborting the service.";
                service_server->setAborted(stream_id); // Set the service to aborted state
                return; // Exit the loop if the service is aborted
            } else {
                LOG(INFO) << "Refused to abort service. Continuing processing.";
                // The service will continue without being aborted
                std::this_thread::sleep_for(std::chrono::seconds(3));
                service_server->setFailed(stream_id);
                return; // Exit the loop and set failed
            }
        }
        // Simulate processing time
        std::this_thread::sleep_for(std::chrono::seconds(1));
        if (count++ > 10) break; // Simple break condition to avoid infinite loop
    }

    // Set the service status to success if it was not aborted
    service_server->setSuccess(stream_id);
    reply->set_data("Reply test");
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        LOG(WARNING) << "Require at least 2 arguments: name, namespace, "
                     << "\nand remember {$namespace$name} should be unique for each node";
        return 1;
    }

    std::shared_ptr<core::NodeHandler> nh = std::make_shared<core::NodeHandler>(argv[1], argv[2]);
    nh->Init();

    // Create a service server that listens for requests on the "hello" topic
    core::ServiceServer<std_msgs::String, std_msgs::String> service_server = nh->serviceServer<std_msgs::String, std_msgs::String>("hello", service_cb);

    // Main loop to keep the program running
    while (core::ok()) {
        std::this_thread::sleep_for(std::chrono::seconds(1)); // Keeps the main thread alive
    }

    return 0;
}
