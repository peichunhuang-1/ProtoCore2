#include "core.hpp"
#include "std.pb.h"

// Global variable to store the latest received message
std_msgs::String string_msg;

// Callback function to handle incoming messages
void cb(const std_msgs::String* msg) {
    string_msg = *msg; // Store the received message in the global variable
}

int main(int argc, char* argv[]) {
    // Check for required arguments
    if (argc < 3) {
        LOG(WARNING) << "Usage: " << argv[0] << " <node_name> <namespace>";
        LOG(WARNING) << "Error: Insufficient arguments. You need to provide a node name and namespace.";
        LOG(WARNING) << "Reminder: The combination of namespace and name (i.e., {$namespace$name}) should be unique for each node.";
        return 1;
    }

    // Create and initialize the NodeHandler with the provided arguments
    std::shared_ptr<core::NodeHandler> nh = std::make_shared<core::NodeHandler>(argv[1], argv[2]);
    nh->Init();

    // Subscribe to the "hello" topic with the callback function
    core::Subscriber sub = nh->subscribe("hello", cb);

    // Set the loop rate to 1 kHz (1000 Hz)
    core::Rate rate(1000);

    // Main loop: process incoming messages and log the data length
    while (core::ok()) {
        nh->spinOnce(); // Receive and process messages, calling the callback if messages are received

        // Log the length of the received message and the thread ID
        LOG(INFO) << "Data: " << string_msg.data();

        // Sleep to maintain the desired loop rate
        rate.sleep();
    }

    return 0;
}
