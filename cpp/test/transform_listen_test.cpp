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
    core::Rate rate(1);
    while(core::ok()) {
        nh->spinOnce();
        LOG(INFO) << "Dynamic configuration";
        tf_listner.lookupTransform("frame_dynamic", "frame_static2", transform);
        LOG(INFO) << transform.transition().x() << ", " << transform.transition().y() << ", " << transform.transition().z();
        LOG(INFO) << transform.rotation().x() << ", " << transform.rotation().y() << ", " << transform.rotation().z() << ", " << transform.rotation().w();
        // transform: {}^{frame_dynamic}_{frame_static2}T
        // the dynamic transform will not reacheable whem the broadcaster close
        LOG(INFO) << "Static configuration";
        tf_listner.lookupTransform("root", "frame_static2", transform); 
        LOG(INFO) << transform.transition().x() << ", " << transform.transition().y() << ", " << transform.transition().z();
        LOG(INFO) << transform.rotation().x() << ", " << transform.rotation().y() << ", " << transform.rotation().z() << ", " << transform.rotation().w();
        // transform: {}^{root}_{frame_static2}T, the static transform can be reached once a node is alive, 
        // even the original broadcaster has close
        rate.sleep();
    }
}