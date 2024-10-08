#include "core.hpp"
#include "std.pb.h"
std::string param_str;
void cb(const std_msgs::String msg) {
    // callback is not recommended to be blocked
    param_str = msg.data();
    LOG(INFO) << "callback trigger, the value is changed to: " << param_str;
}

void cb_const(const std_msgs::String msg) {
    LOG(INFO) << "this should print only once";
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        LOG(WARNING) << "Require at least 2 argument: name, namespace, "
        "\nand remember {$namespace$name} should be unique for each node";
        return 1;
    }
    std_msgs::String string_param; // for initialize param value when declare
    string_param.set_data("this is a constant");

    std::shared_ptr<core::NodeHandler> nh = std::make_shared<core::NodeHandler>(argv[1], argv[2]);
    nh->Init();
    nh->declareParameter("string_param_const", string_param, true); 
    // declare a constant parameter, the parameter belongs to this node, 
    // so the name of this parameter in global view is: argv[2]/argv[1]/string_param_const
    nh->declareParameter("string_param", string_param, false);
    // the name of this parameter in global view is: argv[2]/argv[1]/string_param
    string_param.set_data("changed value");
    nh->setParameter("string_param_const", string_param); 
    // set the parameter value, the value will not change since this is declared as a constant
    nh->setParameter("string_param", string_param);
    // set the parameter value, the value will change, and notify all parameter user call getRemote/get

    if (nh->getParameter("string_param_const", string_param)) LOG(INFO) << "the constant param value is: " << string_param.data();
    // a constant can get, but cannot change by set/setRemote
    if (nh->getParameter("string_param", string_param)) LOG(INFO) << "the param value is: " << string_param.data();
    // get local parameter once, if not decalred, return false, you can also setRemote from other node

    nh->getDynamicParameter("string_param", cb); 
    // will call the callback when parameter changed, there can be multiple callbacks for one parameter, 
    // once you call the getDynamicParameter function, a new handler will be set, 
    // so if you call in a infinite loop, there will be many callbacks working in background, 
    // the constant will be trigger only the first time, it is legal to set dynamic callback to a constant, but no reason to do so
    nh->getDynamicParameter("string_param_const", cb_const); 

    core::Rate rate(10);
    int counter = 0;
    while(core::ok()) {
        LOG(INFO) << "set parameter value to: " << ++counter;
        string_param.set_data(std::to_string(counter));
        nh->setParameter("string_param", string_param);
        rate.sleep();
    }
}