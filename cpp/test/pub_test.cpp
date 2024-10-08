#include "core.hpp"
#include "std.pb.h"

int main(int argc, char* argv[]) {
    if (argc < 3) {
        LOG(WARNING) << "Require at least 2 argument: name, namespace, "
        "\nand remember {$namespace$name} should be unique for each node";
        return 1;
    }
    std_msgs::String string_msg;
    string_msg.set_data(std::string(10000, 'A')); // 1000 x A
    std::shared_ptr<core::NodeHandler> nh = std::make_shared<core::NodeHandler>(argv[1], argv[2]);
    nh->Init();
    core::Publisher<std_msgs::String> pub = nh->advertise<std_msgs::String>("hello");
    core::Rate rate(1000);
    while(core::ok()) {
        nh->spinOnce(); // if you have no subscriber this is optional.
        pub.publish(string_msg);
        LOG(INFO) << "sending in 1kHz!";
        rate.sleep();
    }
}