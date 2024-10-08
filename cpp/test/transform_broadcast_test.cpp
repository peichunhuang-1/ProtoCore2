#include "core.hpp"
#include "std.pb.h"
#include <cmath> // For sin and cos functions

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

    // Define static transforms
    std_msgs::TransformD static_transform1;
    static_transform1.mutable_header()->set_frame_id("root");
    static_transform1.set_child_frame_id("frame_static1");
    static_transform1.mutable_rotation()->set_w(1.0);
    static_transform1.mutable_rotation()->set_z(0.0);
    static_transform1.mutable_transition()->set_x(5);
    static_transform1.mutable_transition()->set_y(0);
    static_transform1.mutable_transition()->set_z(0);
    static_tf.sendTransform(static_transform1);

    std_msgs::TransformD static_transform2;
    static_transform2.mutable_header()->set_frame_id("frame_static1");
    static_transform2.set_child_frame_id("frame_static2");
    static_transform2.mutable_rotation()->set_w(0.707);
    static_transform2.mutable_rotation()->set_z(0.707);
    static_transform2.mutable_transition()->set_x(0);
    static_transform2.mutable_transition()->set_y(5);
    static_transform2.mutable_transition()->set_z(0);
    static_tf.sendTransform(static_transform2);

    std_msgs::TransformD static_transform3;
    static_transform3.mutable_header()->set_frame_id("root");
    static_transform3.set_child_frame_id("frame_static3");
    static_transform3.mutable_rotation()->set_w(0.0);
    static_transform3.mutable_rotation()->set_z(1.0);
    static_transform3.mutable_transition()->set_x(0);
    static_transform3.mutable_transition()->set_y(0);
    static_transform3.mutable_transition()->set_z(5);
    static_tf.sendTransform(static_transform3);

    // Dynamic transforms
    std_msgs::TransformD dynamic_transform;
    dynamic_transform.mutable_header()->set_frame_id("frame_static3");
    dynamic_transform.set_child_frame_id("frame_dynamic");
    
    /*
    Coordinate Tree Structure:
                root
                |
        -----------------
        |               |
    frame_static1    frame_static3
        |               |
    frame_static2     frame_dynamic
    */

    core::Rate rate(1);
    double time = 0.0; // Initialize time variable

    while (core::ok()) {
        nh->spinOnce(); // if you have no subscriber this is optional.
        dynamic_transform.mutable_header()->mutable_timestamp()->set_seconds(time);
        dynamic_transform.mutable_header()->mutable_timestamp()->set_nanos(0);
        // Update position with sine wave for smooth movement
        dynamic_transform.mutable_transition()->set_x(1.0 * sin(time));
        dynamic_transform.mutable_transition()->set_y(1.0 * cos(time));
        dynamic_transform.mutable_transition()->set_z(0.5 * sin(2 * time)); // Vertical oscillation

        // Update rotation over time
        dynamic_transform.mutable_rotation()->set_w(cos(time / 2)); // Change rotation based on time
        dynamic_transform.mutable_rotation()->set_z(sin(time / 2)); // Update z-component of rotation

        // Send the updated dynamic transform
        tf.sendTransform(dynamic_transform);
        
        time += 1; // Increment time
        rate.sleep();
    }
}
