#ifndef SUBSCRIBER_HPP
#define SUBSCRIBER_HPP

namespace core {
    class NodeHandler;
    class Subscriber {
        public:
        Subscriber(const string& topic, shared_ptr<core::NodeHandler> nh) : sub_topic(topic), nh_(nh) {}
        void                                    shutdown();
        private:
        shared_ptr<core::NodeHandler>           nh_;
        const string                            sub_topic;
    };
}

#endif