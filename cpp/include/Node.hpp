#ifndef NODE_HPP
#define NODE_HPP
#include <unordered_set>
#include <map>
using namespace std;
class Node {
    public:
    Node(const string& name);
    ~Node();
    bool                                has_subscribed_topic(const string& name, const string& ip, const int& port);
    bool                                add_subscribed_topic(const string& name);
    bool                                delete_subscribed_topic(const string& name);

    bool                                has_serving_service(const string& name);
    bool                                add_serving_service(const string& name);
    bool                                delete_serving_service(const string& name);

    bool                                has_possess_param(const string& name);
    bool                                add_possess_param(const string& name);
    bool                                delete_possess_param(const string& name);

    bool                                has_possess_tree(const string& name);
    bool                                add_possess_tree(const string& name);
    bool                                delete_possess_tree(const string& name);

    private:    
    const string                        name;
    const string                        ip;
    const int                           port;
    unordered_set<string>               subscribed_topics;
    unordered_set<string>               serving_services;
    unordered_set<string>               possess_params;
    unordered_set<string>               possess_trees;
};

class NodePeers {
    public:
    NodePeers();
    void                                add_node(const string& node);
    void                                delete_node(const string& node);

    void                                add_subscribed_topic(const string& node, const string& topic);
    void                                add_serving_service(const string& node, const string& service);
    void                                add_possess_param(const string& node, const string& param);
    void                                add_possess_tree(const string& node, const string& param);

    void                                delete_subscribed_topic(const string& node, const string& topic);
    void                                delete_serving_service(const string& node, const string& service);
    void                                delete_possess_param(const string& node, const string& param);
    void                                delete_possess_tree(const string& node, const string& param);

    void                                query_topic(const string& topic, vector<Node> nodes);

    private:
    unordered_map<int, Node>            nodes;
    unordered_map<string, int>          nodes_id;
    /*
    prefix of token
    sct: subscribed topic
    svs: serving service
    psp: possess param
    psr: possess tree
    token = prefix + object name
    */
    map<string, unordered_set<int>>     tokens; 
};

#endif