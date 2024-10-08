#include "core.hpp"
#include "std.pb.h"

std::string param_str;

// Callback function for dynamic parameter changes
void cb(const std_msgs::String msg) {
    param_str = msg.data();
    LOG(INFO) << "Callback triggered: the value is changed to: " << param_str;
}

// Callback function for constant parameter (should print only once)
void cb_const(const std_msgs::String msg) {
    LOG(INFO) << "This should print only once";
}

int main(int argc, char* argv[]) {
    // Check for required arguments
    if (argc < 3) {
        LOG(WARNING) << "Usage: " << argv[0] << " <node_name> <namespace>";
        LOG(WARNING) << "Error: Insufficient arguments. You need to provide a node name and namespace.";
        LOG(WARNING) << "Reminder: The combination of namespace and name (i.e., {$namespace$name}) should be unique for each node.";
        return 1;
    }

    // Initialize the string parameter
    std_msgs::String string_param; 
    string_param.set_data("this is a constant");

    // Create and initialize the NodeHandler
    std::shared_ptr<core::NodeHandler> nh = std::make_shared<core::NodeHandler>(argv[1], argv[2]);
    nh->Init();

    // Declare parameters
    nh->declareParameter("string_param_const", string_param, true); // Constant parameter
    nh->declareParameter("string_param", string_param, false); // Dynamic parameter

    // Set initial values for parameters
    nh->setParameter("string_param_const", string_param); // This won't change
    string_param.set_data("changed value");
    nh->setParameter("string_param", string_param); // This will change

    // Log the parameter values
    if (nh->getParameter("string_param_const", string_param)) {
        LOG(INFO) << "The constant param value is: " << string_param.data();
    }
    if (nh->getParameter("string_param", string_param)) {
        LOG(INFO) << "The dynamic param value is: " << string_param.data();
    }

    // Set dynamic callbacks for parameter changes
    nh->getDynamicParameter("string_param", cb);
    nh->getDynamicParameter("string_param_const", cb_const); // Triggered only once

    // Main loop
    core::Rate rate(1);
    std::string user_input;
    while (core::ok()) {
        LOG(INFO) << "Current parameter value: " << string_param.data();

        // Prompt user for new parameter value
        LOG(INFO) << "Enter a new value for 'string_param' (or type 'exit' to quit): ";
        std::getline(std::cin, user_input);

        if (user_input == "exit") {
            LOG(INFO) << "Exiting the program.";
            break; // Exit the loop if the user types 'exit'
        }

        // Set the new parameter value based on user input
        string_param.set_data(user_input);
        nh->setParameter("string_param", string_param);
        LOG(INFO) << "Parameter value set to: " << string_param.data();

        rate.sleep(); // Maintain the loop rate
    }

    return 0;
}
