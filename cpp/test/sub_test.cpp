#include "core.hpp"
#include "std.pb.h"
std_msgs::String string_msg;
void cb(const std_msgs::String* msg) {
    LOG(INFO) << "receive data! you can check the timestamp and valid the pub/sub frequency" << ", print in thread: " << std::this_thread::get_id();
    string_msg = *msg;
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        LOG(WARNING) << "Require at least 2 argument: name, namespace, "
        "\nand remember {$namespace$name} should be unique for each node";
        return 1;
    }
    std::shared_ptr<core::NodeHandler> nh = std::make_shared<core::NodeHandler>(argv[1], argv[2]);
    nh->Init();
    core::Subscriber sub = nh->subscribe("hello", cb);
    core::Rate rate(1000);
    while(core::ok()) {
        nh->spinOnce(); 
        // receive, deserialize messages, and call the callback function
        // if you get 100 messages in this period, then it process 100 message at a time, 
        // so basically the callback should not blocked, or spinOnce may be blocked there.
        LOG(INFO) << "data length (should be 1000): " << string_msg.data().length() << ", print in thread: " << std::this_thread::get_id();
        rate.sleep();
    }
}