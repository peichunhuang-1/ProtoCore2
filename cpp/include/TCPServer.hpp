#ifndef TCP_SERVER_HPP
#define TCP_SERVER_HPP
#include <errno.h>
#include <thread>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <deque>
#include <vector>
#include <map>
#include <set>
#include <cstdio>
#include "serialization.hpp"
#include "AsyncSocket.hpp"

namespace core {
using namespace std;
class TCPServer final: public Socket {
    public:
    TCPServer(const string& ip, const int port = 0);
    int                                     init_tcp_srv(); 
    void                                    accept_client(const string& topic, const string& ip, const int &port);
    int                                     event_handler(int timeout = 0); // do all event in event pool
    unordered_map<string, Decoder*>         decoders;
    private:
    unordered_map<string, string>           addr_to_topic;
    vector<string>                          fd_to_addr;
    vector<string>                          fd_receive_data;
    unordered_map<string, int>              unmapped_addr_to_fd;
    void                                    handle_client_event(const int& client_fd, const int& revents); 
    int                                     accept_new_client();
    int                                     tcp_srv_fd;
    const string                            tcp_srv_ip;
    int                                     tcp_srv_port;
    const int                               max_buffer_size = 65536;

    int                                     bind_socket(const int& fd, const sockaddr_in& addr);
    int                                     listen_socket(const int& fd);
    int                                     get_socket_info(const int& fd);
    void                                    close_and_delete_event(const int& fd, const int& revents);
    shared_mutex                            mtx;
};
}

#endif