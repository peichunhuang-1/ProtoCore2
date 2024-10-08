#ifndef PUBLISHER_HPP
#define PUBLISHER_HPP

namespace core {
    class NodeHandler;
    template<typename T>
    class Publisher {
        public:
        Publisher(const string& topic, shared_ptr<core::NodeHandler> nh);
        void                                publish(const T& msg, bool cache = false);
        void                                shutdown();
        private:
        shared_ptr<core::NodeHandler>       nh_;
        const string                        pub_topic;
    };
}

#endif