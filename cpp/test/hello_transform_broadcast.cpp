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
    core::TransformBroadcaster tf = nh->tfBroadcaster();
    core::StaticTransformBroadcaster static_tf = nh->tfStaticBroadcaster();
    std_msgs::TransformD transform;
    transform.mutable_header()->set_frame_id("root");
    transform.set_child_frame_id("frame_static");
    transform.mutable_rotation()->set_w(0.707);
    transform.mutable_rotation()->set_z(0.707);
    transform.mutable_transition()->set_x(10);
    static_tf.sendTransform(transform);

    transform.mutable_header()->set_frame_id("frame_static");
    transform.set_child_frame_id("frame_dynamic");
    transform.mutable_rotation()->set_w(0.707);
    transform.mutable_rotation()->set_z(0.707);
    transform.mutable_transition()->set_x(10);
    core::Rate rate(10);
    while(core::ok()) {
        nh->spinOnce(); // if you have no subscriber this is optional.
        tf.sendTransform(transform);
        rate.sleep();
    }
}