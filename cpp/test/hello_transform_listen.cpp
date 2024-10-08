#include "core.hpp"
#include "std.pb.h"

int main(int argc, char* argv[]) {
    if (argc < 3) {
        LOG(WARNING) << "Require at least 2 argument: name, namespace, "
        "\nand remember {$namespace$name} should be unique for each node";
        return 1;
    }
    std::shared_ptr<core::NodeHandler> nh = std::make_shared<core::NodeHandler>(argv[1], argv[2]);
    nh->Init();
    core::TransformListener tf_listner = nh->tfListener();
    std_msgs::TransformD transform;
    core::Rate rate(10);
    while(core::ok()) {
        nh->spinOnce();
        tf_listner.lookupTransform("root", "frame_dynamic", transform); // transform: {}^{root}_{frame_dynamic}T
        LOG(INFO) << "Dynamic configuration";
        LOG(INFO) << transform.transition().x() << ", " << transform.transition().y() << ", " << transform.transition().z();
        LOG(INFO) << transform.rotation().x() << ", " << transform.rotation().y() << ", " << transform.rotation().z() << ", " << transform.rotation().w();
        tf_listner.lookupTransform("root", "frame_static", transform); // transform: {}^{root}_{frame_static}T
        LOG(INFO) << "Static configuration";
        LOG(INFO) << transform.transition().x() << ", " << transform.transition().y() << ", " << transform.transition().z();
        LOG(INFO) << transform.rotation().x() << ", " << transform.rotation().y() << ", " << transform.rotation().z() << ", " << transform.rotation().w();
        rate.sleep();
    }
}