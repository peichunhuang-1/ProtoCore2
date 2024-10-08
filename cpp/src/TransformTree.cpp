#include "rscl.hpp"

namespace core {
bool TransformTreeNode::TransformEntry::operator<(const TransformEntry& other) const {
    return transform.header().timestamp().seconds() < other.transform.header().timestamp().seconds() ||
    (transform.header().timestamp().seconds() == other.transform.header().timestamp().seconds() && 
    transform.header().timestamp().nanos() < other.transform.header().timestamp().nanos());
}

KDL::Frame TransformTreeNode::TransformEntry::toKDL() const {
    const auto& translation = transform.transition();
    const auto& rotation = transform.rotation();
    KDL::Frame frame;
    frame.p = KDL::Vector(translation.x(), translation.y(), translation.z());
    frame.M = KDL::Rotation::Quaternion(rotation.x(), rotation.y(), rotation.z(), rotation.w());
    return frame;
}

TransformTreeNode::TransformTreeNode(const string& id, const int memory_duration_ms) :
frame_id(id), memory_duration(memory_duration_ms) {
    parent = nullptr;
}

bool TransformTreeNode::transform(KDL::Frame& frame, const google::protobuf::Timestamp least_stamp, int tolerance_ms) {
    if (value_time_series.empty()) return false;
    if (is_static || tolerance_ms < 0) {
        frame = value_time_series.rbegin()->toKDL();
        return true;
    }
    std_msgs::TransformD time_token; 
    time_token.mutable_header()->mutable_timestamp()->CopyFrom(least_stamp);
    TransformEntry token {time_token};
    auto find_least_ptr = value_time_series.lower_bound(token);
    if (find_least_ptr == value_time_series.end()) find_least_ptr = prev(value_time_series.end());

    auto found_transform = *find_least_ptr;

    int64_t time_diff_ms = (found_transform.transform.header().timestamp().seconds() - least_stamp.seconds()) * 1000 
                        + (found_transform.transform.header().timestamp().nanos() - least_stamp.nanos()) / 1000;
    if (abs(time_diff_ms) <= tolerance_ms) {
        frame = found_transform.toKDL();
        return true;
    }
    return false; 
}

void TransformTreeNode::cleanup(google::protobuf::Timestamp current_time) {
    if (is_static) return;
    auto time_diff_ms = [](const google::protobuf::Timestamp& l, const google::protobuf::Timestamp& r) {
        return (r.seconds() - l.seconds()) * 1000 
                + (r.nanos() - l.nanos()) / 1000;
    };
    auto it = value_time_series.begin();
    while (it != value_time_series.end()) {
        if (time_diff_ms(current_time, it->transform.header().timestamp()) > memory_duration ) {
            it = value_time_series.erase(it);
        } else {
            ++it;
        }
    }
}

void TransformTreeNode::push(const std_msgs::TransformD transform) {
    value_time_series.insert({transform});
    google::protobuf::Timestamp current_time = value_time_series.rbegin()->transform.header().timestamp();
    cleanup(current_time);
}

void TransformTree::addTransformNode(const std_msgs::TransformD transform, bool is_static) {
    const string child_frame_id = transform.child_frame_id();
    const string frame_id = transform.header().frame_id();
    if (!nodes.count(child_frame_id)) {
        nodes[child_frame_id] = new TransformTreeNode(child_frame_id);
    }
    if (!nodes.count(frame_id)) {
        nodes[frame_id] = new TransformTreeNode(frame_id);
    }
    if (is_static) nodes[child_frame_id]->is_static = true;
    nodes[child_frame_id]->push(transform);
    if (!nodes[child_frame_id]->parent) {
        nodes[child_frame_id]->parent = nodes[frame_id];
        return ;
    } 
    if (nodes[child_frame_id]->parent->frame_id != frame_id) {
        LOG(ERROR) << "duplicated parent node for tf: " << child_frame_id; 
    }
}

bool TransformTree::transform(KDL::Frame& frame, const string from, const string to, const google::protobuf::Timestamp least_stamp, int tolerance_ms) {
    const string token = from > to? from + ":" + to : to + ":" + from;
    auto ancestor_ptr = shared_ancestor.find(token);
    if (ancestor_ptr != shared_ancestor.end()) {
        return get_transform(frame, from, to, least_stamp, tolerance_ms);
    }
    if (!find_shared_ancestor(from, to)) {
        return false;
    }
    return get_transform(frame, from, to, least_stamp, tolerance_ms);
}

bool TransformTree::find_shared_ancestor(const string from, const string to) {
    if (!nodes.count(from) || !nodes.count(to)) {
        LOG(WARNING) << "one or both frames are missing in the tree: " << from << " or " << to;
        return false;
    }
    const string token = from > to? from + ":" + to : to + ":" + from;
    unordered_set<TransformTreeNode*> from_traversed_nodes;
    TransformTreeNode* root = nodes[from];
    while (root) {
        if ( !from_traversed_nodes.insert(root).second ) {
            LOG(WARNING) << "closed loop in tree";
            return false;
        }
        root = root->parent;
    }
    unordered_set<TransformTreeNode*> to_traversed_nodes;
    root = nodes[to];
    while (root) {
        if ( !to_traversed_nodes.insert(root).second ) {
            LOG(WARNING) << "closed loop in tree";
            return false;
        }
        if ( from_traversed_nodes.count(root) ) {
            shared_ancestor[token] = root;
            break;
        }
        root = root->parent;
    }
    LOG(INFO) << "find shared ancestor " << root->frame_id;
    return true;
}

bool TransformTree::get_transform(KDL::Frame& frame, const string from, const string to, const google::protobuf::Timestamp least_stamp, int tolerance_ms) {
    const string token = from > to? from + ":" + to : to + ":" + from;
    frame = KDL::Frame::Identity();
    TransformTreeNode* root = shared_ancestor[token];
    TransformTreeNode* current_node = nodes[from];
    while (current_node && (current_node != root)) {
        KDL::Frame current_transform;
        if (!current_node->transform(current_transform, least_stamp, tolerance_ms)) return false;
        frame = current_transform * frame;
        current_node = current_node->parent;
    }
    frame = frame.Inverse();
    current_node = nodes[to];
    while (current_node && (current_node != root)) {
        KDL::Frame current_transform;
        if (!current_node->transform(current_transform, least_stamp, tolerance_ms)) return false;
        frame = current_transform * frame;
        current_node = current_node->parent;
    }
    return true;
}

TransformBroadcaster::TransformBroadcaster(tf_publisher tf_pub) : pub(tf_pub) {}
void TransformBroadcaster::sendTransform(const std_msgs::TransformD& tf) {
    pub->publish(tf);
}

StaticTransformBroadcaster::StaticTransformBroadcaster(tf_publisher static_tf_pub) : pub(static_tf_pub) {}

void StaticTransformBroadcaster::sendTransform(const std_msgs::TransformD& tf) {
    pub->publish(tf, true);
}

TransformListener::TransformListener(shared_ptr<NodeHandler> nh, tf_publisher pub) : nh_(nh), tf_static_pub(pub) {
    nh_->subscribe("/tf", function<void(const std_msgs::TransformD*)>(
        std::bind(&TransformListener::tf_call_back, this, std::placeholders::_1)));
    tf_sub = make_shared<Subscriber>("/tf", nh_);
    nh_->subscribe("/tf_static", function<void(const std_msgs::TransformD*)>(
        std::bind(&TransformListener::static_tf_call_back, this, std::placeholders::_1)));
    tf_static_sub = make_shared<Subscriber>("/tf_static", nh_);
}

void TransformListener::tf_call_back(const std_msgs::TransformD* transform) {
    tf_tree.addTransformNode(*transform);
}
void TransformListener::static_tf_call_back(const std_msgs::TransformD* transform) {
    tf_tree.addTransformNode(*transform, true);
    tf_static_pub->publish(*transform, true);
}

bool TransformListener::lookupTransform(const string& from_tf, const string& to_tf, std_msgs::TransformD &transform, const int timeout_ms) {
    KDL::Frame frame;
    bool ret = tf_tree.transform(frame, from_tf, to_tf, transform.header().timestamp(), timeout_ms);
    if (ret) {
        transform.mutable_header()->set_frame_id(from_tf);
        transform.set_child_frame_id(to_tf);
        transform.mutable_transition()->set_x(frame.p.x());
        transform.mutable_transition()->set_y(frame.p.y());
        transform.mutable_transition()->set_z(frame.p.z());
        double w, x, y, z;
        frame.M.GetQuaternion(x, y, z, w);
        transform.mutable_rotation()->set_x(x);
        transform.mutable_rotation()->set_y(y);
        transform.mutable_rotation()->set_z(z);
        transform.mutable_rotation()->set_w(w);
    }
    return ret;
}

TransformBroadcaster NodeHandler::tfBroadcaster() {
    if(!tf_pub) tf_pub = make_shared<Publisher<std_msgs::TransformD>>(advertise<std_msgs::TransformD>("/tf"));
    return TransformBroadcaster(tf_pub);
}

StaticTransformBroadcaster NodeHandler::tfStaticBroadcaster() {
    if(!static_tf_pub) static_tf_pub = make_shared<Publisher<std_msgs::TransformD>>(advertise<std_msgs::TransformD>("/tf_static"));
    return StaticTransformBroadcaster(static_tf_pub);
}

TransformListener NodeHandler::tfListener() {
    if(!static_tf_pub) static_tf_pub = make_shared<Publisher<std_msgs::TransformD>>(advertise<std_msgs::TransformD>("/tf_static"));
    return TransformListener(shared_from_this(), static_tf_pub);
}
}
