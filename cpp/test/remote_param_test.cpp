#include "core.hpp"
#include "std.pb.h"

std::string param_str;

// Callback function to handle changes to the remote parameter
void cb(const std_msgs::String msg) {
    param_str = msg.data();
    LOG(INFO) << "Callback triggered: the remote parameter value is changed to: " << param_str;
}

int main(int argc, char* argv[]) {
    // Check for the required number of arguments
    if (argc < 3) {
        LOG(WARNING) << "Usage: " << argv[0] << " <node_name> <namespace>";
        LOG(WARNING) << "Error: Insufficient arguments. You need to provide a node name and namespace.";
        LOG(WARNING) << "Reminder: The combination of namespace and name (i.e., {$namespace$name}) should be unique for each node.";
        return 1;
    }

    // Initialize the string parameter
    std_msgs::String string_param;

    // Create and initialize the NodeHandler
    std::shared_ptr<core::NodeHandler> nh = std::make_shared<core::NodeHandler>(argv[1], argv[2]);
    nh->Init();

    // Prompt user for the owner of the parameter
    std::string param_owner_name;
    LOG(INFO) << "Enter the node name that owns the parameter 'string_param': ";
    std::getline(std::cin, param_owner_name);

    // Get the remote parameter initially
    std_msgs::String param;
    core::ParameterStatus status = core::ParameterStatus::TMEP_NOT_AVALIABLE;
    
    // Attempt to retrieve the remote parameter until it's available
    while ((status != core::ParameterStatus::NORMAL) && core::ok()) {
        status = nh->getRemoteParameter("string_param", param, param_owner_name);
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    // Log the retrieved parameter value
    LOG(INFO) << "Parameter retrieved: " << param.data();

    // Set up a dynamic callback for remote parameter changes
    status = nh->getRemoteDynamicParameter("string_param", cb, param_owner_name); 
    // Note: You can call getRemoteDynamicParameter multiple times, but the new callback will replace the old one.

    core::Rate rate(1);
    while (core::ok()) {
        // Prompt user for a new value
        std::string user_input;
        LOG(INFO) << "Enter a new value for 'string_param' (or type 'exit' to quit): ";
        std::getline(std::cin, user_input);

        // Check if the user wants to exit
        if (user_input == "exit") {
            LOG(INFO) << "Exiting the program.";
            break; // Exit the loop if the user types 'exit'
        }

        // Update the parameter value based on user input
        string_param.set_data(user_input);
        nh->setRemoteParameter("string_param", string_param, param_owner_name);
        
        // Set constant parameter as well, but you can set it to a static value or remove if not needed
        nh->setRemoteParameter("string_param_const", string_param, param_owner_name);
        
        LOG(INFO) << "Remote parameter 'string_param' set to: " << string_param.data();
        rate.sleep(); // Maintain the loop rate
    }

    return 0;
}
