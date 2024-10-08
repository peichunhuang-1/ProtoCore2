#include "core.hpp"
#include "std.pb.h"
std::string param_str;
void cb(const std_msgs::String msg) {
    // callback is not recommended to be blocked
    param_str = msg.data();
    LOG(INFO) << "callback trigger, the remote parameter value is changed to: " << param_str;
}

int main(int argc, char* argv[]) {
    if (argc < 4) {
        LOG(WARNING) << "Require at least 3 argument: name, namespace, the parameter namespace(the owner node name)"
        "\nand remember {$namespace$name} should be unique for each node";
        return 1;
    }
    
    std_msgs::String string_param;

    std::shared_ptr<core::NodeHandler> nh = std::make_shared<core::NodeHandler>(argv[1], argv[2]);
    nh->Init();

    std_msgs::String param;
    core::ParameterStatus status = core::ParameterStatus::TMEP_NOT_AVALIABLE;
    while ((status != core::ParameterStatus::NORMAL) && core::ok()) {
        status = nh->getRemoteParameter("string_param", param, argv[3]);
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    // if the node named $argv[3] was not ready, or there is no parameter named 'string_param', $status would be TMEP_NOT_AVALIABLE
    LOG(INFO) << "param get: " << param.data();

    status = nh->getRemoteDynamicParameter("string_param", cb, argv[3]); 
    // you can call getRemoteDynamicParameter more than one time, but the new callback will replace the old one, 

    core::Rate rate(1);
    int counter = 0;
    while(core::ok()) {
        LOG(INFO) << "set parameter value to: " << ++counter;
        string_param.set_data(std::to_string(counter));
        nh->setRemoteParameter("string_param", string_param, argv[3]);
        nh->setRemoteParameter("string_param_const", string_param, argv[3]);
        rate.sleep();
    }
}