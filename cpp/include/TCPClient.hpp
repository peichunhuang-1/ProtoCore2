#ifndef TCP_CLIENT_HPP
#define TCP_CLIENT_HPP
#include <errno.h>
#include <thread>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <deque>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <cstdio>
#include "AsyncSocket.hpp"

#define MAX_WRITE_BUFFER_SIZE                           65536

namespace core{
using namespace std;
class NodeHandler;
class client_info;
class TCPClient final: public Socket {
    public:
    TCPClient();
    client_info                                         add_client(const string& topic, const string& ip, const int& port);
    void                                                write_to_socket(const string& topic, const string& msg, const int& timeout = 0);
    bool                                                write_to_cache(const string& topic, const string& data);
    private:
    unordered_map<string, unordered_set<int>>           clients_topic_fd;
    vector<string>                                      clients_data;
    void                                                close_and_delete_event(const int& fd);
    bool                                                get_socket_info(const int& fd, string& src_ip, int& src_port);   
    bool                                                write_fd(const string& topic, const int& fd);
    shared_mutex                                        mtx;             
    unordered_map<string, unordered_set<string>>        static_topic_cache;         
}; 
} 


#endif