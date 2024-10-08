#ifndef TRANSFORM_TREE_HPP
#define TRANSFORM_TREE_HPP
#include "std.pb.h"
#include <type_traits>
#include <kdl/frames.hpp>
#include <glog/logging.h>
#include <google/protobuf/util/time_util.h>

namespace core {

using namespace std;

class TransformTreeNode {
    public:
    TransformTreeNode(const string& id, const int memory_duration_ms = 1000);
    TransformTreeNode*                  parent;
    bool                                transform(KDL::Frame& frame, const google::protobuf::Timestamp least_stamp, int tolerance_ms);
    void                                push(const std_msgs::TransformD transform);
    const string                        frame_id;
    bool                                is_static = false;
    private:

    struct TransformEntry {
        std_msgs::TransformD            transform;
        bool                            operator<(const TransformEntry& other) const ;
        KDL::Frame                      toKDL() const;
    };

    set<TransformEntry>                 value_time_series;
    const int                           memory_duration;
    void                                cleanup(google::protobuf::Timestamp current_time);
};

class TransformTree {
    public:
    TransformTree()                   = default;
    void                                addTransformNode(const std_msgs::TransformD node, bool is_static = false);
    bool                                transform(KDL::Frame& frame, const string from, const string to, const google::protobuf::Timestamp least_stamp, int tolerance_ms = -1);
    // case 1: no tolerance set (-1), would get the latest timestamp
    // case 2: tolerance set, would get the timestamp whom been in tolerance
    private:
    map<string, TransformTreeNode*>     nodes;
    bool                                valid;
    map<string, TransformTreeNode*>     shared_ancestor;
    bool                                find_shared_ancestor(const string from, const string to);
    bool                                get_transform(KDL::Frame& frame, const string from, const string to, const google::protobuf::Timestamp least_stamp, int tolerance_ms);
};

class TransformBroadcaster {
    public:
    using tf_publisher = shared_ptr<Publisher<std_msgs::TransformD>>;
    TransformBroadcaster(tf_publisher tf_pub) ;
    void                                sendTransform(const std_msgs::TransformD& tf);
    private:
    tf_publisher                        pub;
};

class StaticTransformBroadcaster {
    public:
    using tf_publisher = shared_ptr<Publisher<std_msgs::TransformD>>;
    StaticTransformBroadcaster(tf_publisher static_tf_pub);
    void                                sendTransform(const std_msgs::TransformD& tf);
    private:
    tf_publisher                        pub;
};

class TransformListener {
    public:
    using tf_publisher = shared_ptr<Publisher<std_msgs::TransformD>>;
    TransformListener(shared_ptr<NodeHandler> nh, tf_publisher pub);
    bool                                lookupTransform(const string& from_tf, const string& to_tf, std_msgs::TransformD &transform, const int timeout_ms = -1);
    // bool                                waitForTransform(const string& from_tf, const string& to_tf, TransformD &transform, const int timeout_ms = -1);
    private:
    TransformTree                       tf_tree;
    shared_ptr<NodeHandler>             nh_;
    shared_ptr<Subscriber>              tf_sub;
    shared_ptr<Subscriber>              tf_static_sub;
    tf_publisher                        tf_static_pub;
    void                                tf_call_back(const std_msgs::TransformD* transform);
    void                                static_tf_call_back(const std_msgs::TransformD* transform);
};

}

#endif