#include "core.hpp"
#include "std.pb.h"

int main(int argc, char* argv[]) {
    // Check for the required arguments
    if (argc < 3) {
        LOG(WARNING) << "Usage: " << argv[0] << " <node_name> <namespace>";
        LOG(WARNING) << "Error: Insufficient arguments. You need to provide a node name and namespace.";
        LOG(WARNING) << "Reminder: The combination of namespace and name (i.e., {$namespace$name}) should be unique for each node.";
        return 1;
    }

    // Create and initialize the NodeHandler with the provided arguments
    std::shared_ptr<core::NodeHandler> nh = std::make_shared<core::NodeHandler>(argv[1], argv[2]);
    nh->Init();

    // Set up a publisher for the "hello" topic
    core::Publisher<std_msgs::String> pub = nh->advertise<std_msgs::String>("hello");

    // Set the loop rate to 1 kHz (1000 Hz)
    core::Rate rate(1000);

    // Initialize a counter to track the message count
    int count = 0;

    // Main loop: publish incrementing count at 1 kHz
    while (core::ok()) {
        nh->spinOnce(); // Optional if there are no subscribers

        // Create a message with the current count
        std_msgs::String string_msg;
        string_msg.set_data(std::to_string(count++));

        // Publish the message
        pub.publish(string_msg);

        // Log more detailed output for better clarity
        LOG(INFO) << "Publishing message with count value: " << count - 1 << " to topic 'hello' at 1 kHz rate.";

        // Sleep to maintain the desired loop rate
        rate.sleep();
    }

    return 0;
}
