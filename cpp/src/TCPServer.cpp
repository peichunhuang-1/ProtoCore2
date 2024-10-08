#include "TCPServer.hpp"
namespace core {
TCPServer::TCPServer(const string& ip, const int port) : tcp_srv_ip(ip), tcp_srv_port(port) {
    #ifdef __linux__
    init_epoll(); // initialize epoll
    #elif __APPLE__
    init_kqueue(); // initialize kqueue
    #endif
    fd_to_addr.resize(100);
}

void TCPServer::accept_client(const string& topic, const string& ip, const int &port) {
    const string token = ip + ":" + to_string(port);
    if (unmapped_addr_to_fd.count(token)) {
        unique_lock<shared_mutex> lock(mtx);
        LOG(INFO) << "accept topic publisher on: " << token;
        addr_to_topic[token] = topic;
        int fd = unmapped_addr_to_fd[token];
        if (fd_receive_data.size() <= fd) fd_receive_data.resize(fd_receive_data.size() + 100);
        fd_receive_data[fd] = "";
        #ifdef __linux__
        add_epoll_event(fd, EPOLLIN | EPOLLPRI);
        #elif __APPLE__
        add_kqueue_event(fd, EVFILT_READ, EV_ADD | EV_ENABLE);
        #endif
        unmapped_addr_to_fd.erase(token);
    } else {
        this_thread::sleep_for(chrono::milliseconds(100));
        accept_client(topic, ip, port);
    }
}

int TCPServer::init_tcp_srv() {
    struct sockaddr_in addr;
    tcp_srv_fd = create_socket();
    if (tcp_srv_fd < 0) return -1;

    memset(&addr, 0, sizeof(addr)); // prevent undefine behavior
    addr.sin_family = AF_INET;
    addr.sin_port = htons(tcp_srv_port);
    addr.sin_addr.s_addr = inet_addr(tcp_srv_ip.c_str());

    if (bind_socket(tcp_srv_fd, addr) < 0) return -1;
    if (listen_socket(tcp_srv_fd) < 0) return -1;
    if (get_socket_info(tcp_srv_fd) < 0) return -1;

#ifdef __linux__
    if (add_epoll_event(tcp_srv_fd, EPOLLIN | EPOLLPRI | EPOLLERR) < 0) return -1;
#elif __APPLE__
    if (add_kqueue_event(tcp_srv_fd, EVFILT_READ, EV_ADD | EV_ENABLE) < 0) return -1;
#endif

    return tcp_srv_port;
}

int TCPServer::bind_socket(const int& fd, const sockaddr_in& addr) {
    if (::bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        LOG(ERROR) << "Failed to bind acceptor socket";
        close(fd);
        return -1;
    }
    return 0;
}

int TCPServer::listen_socket(const int& fd) {
    if (listen(fd, 10) < 0) {
        LOG(ERROR) << "Failed to listen on acceptor socket";
        close(fd);
        return -1;
    }
    return 0;
}

int TCPServer::get_socket_info(const int& fd) {
    struct sockaddr_in addr;
    socklen_t addr_len = sizeof(addr);
    if (getsockname(fd, (struct sockaddr*)&addr, &addr_len) == -1) {
        LOG(ERROR) << "Failed to get socket info";
        close(fd);
        return -1;
    }
    tcp_srv_port = ntohs(addr.sin_port);
    return 0;
}


int TCPServer::accept_new_client() {
    struct sockaddr_in addr;
    socklen_t addr_len = sizeof(addr);

    memset(&addr, 0, sizeof(addr));

    int client_fd = accept(tcp_srv_fd, (struct sockaddr *)&addr, &addr_len);
    if (client_fd < 0) {
        if (errno == EAGAIN) return 0;
        LOG(ERROR) << "Failed to accept client " << errno ;
        return -1;
    }
    if (set_nonblock(client_fd) < 0) {
        close(client_fd);
        return -1;
    }
    int src_port = ntohs(addr.sin_port);
    char src_ip_buf[15];
    string src_ip = string(inet_ntop(AF_INET, &addr.sin_addr, src_ip_buf, sizeof(src_ip_buf)));

    if (fd_to_addr.size() <= client_fd) fd_to_addr.resize(fd_to_addr.size() + 100);
    const string token = src_ip + ":" + to_string(src_port);
    fd_to_addr[client_fd] = token;
    unmapped_addr_to_fd[token] = client_fd;
    LOG(INFO) << "TCP connection accepted, client info: " << token;
}

void TCPServer::close_and_delete_event(const int& fd, const int& revents) {
    const string token = fd_to_addr[fd];
    if (unmapped_addr_to_fd.count(token)) unmapped_addr_to_fd.erase(token);
    if (addr_to_topic.count(token)) addr_to_topic.erase(token);
    fd_to_addr[fd] = "";
    fd_receive_data[fd] = "";
#ifdef __linux__
    delete_epoll_event(fd);
#elif __APPLE__
    delete_kqueue_event(fd, revents);
#endif
    close(fd);
    return;
}

void TCPServer::handle_client_event(const int& client_fd, const int& revents) {
    #ifdef __linux__
        const uint32_t err_mask = EPOLLERR | EPOLLHUP;
        if (revents & err_mask) {
            LOG(INFO) << "Client " << fd_to_addr[client_fd] << " has closed its connection" ;
            return close_and_delete_event(client_fd, revents);
        }
    #elif __APPLE__
        if (!(revents & EVFILT_READ)) return;
    #endif
    const string client_addr = fd_to_addr[client_fd];
    if (!addr_to_topic.count(client_addr)) return;
    const string topic = addr_to_topic[client_addr];
    if (!decoders.count(topic)) return;

    ssize_t recv_ret;
    static size_t buf_size = 8192;
    char buffer_once[buf_size];
    while (true) {
        recv_ret = recv(client_fd, buffer_once, sizeof(buffer_once), 0);
        if (recv_ret == 0) {
            LOG(ERROR) << "Error receiving empty data";
            return close_and_delete_event(client_fd, revents);
        }
        if (recv_ret < 0) {
            if (errno == EAGAIN) return;
            LOG(ERROR) << "Error receiving data " << errno;
            return close_and_delete_event(client_fd, revents);
        }
        else {
            fd_receive_data[client_fd].append(buffer_once, recv_ret);
            int delete_size = decoders[topic]->decode(fd_receive_data[client_fd]);
            decoders[topic]->handle();
            fd_receive_data[client_fd].erase(0, delete_size);
            buf_size = buf_size == recv_ret? buf_size * 2: buf_size;
            buf_size = buf_size > max_buffer_size? max_buffer_size: buf_size;
        }
    }
}

int TCPServer::event_handler(int timeout) {
    unique_lock<shared_mutex> lock(mtx);
    int ret = 0;
    int event_ret;
    int fd;
#ifdef __linux__
    event_ret = epoll_wait(epoll_fd, events, maxevents, timeout);
#elif __APPLE__
    struct timespec ts = { timeout / 1000, (timeout % 1000) * 1000000 };
    event_ret = kevent(kq_fd, NULL, 0, events, maxevents, &ts);
#endif 
    if (event_ret == 0) return 0;

    if (event_ret == -1) {
        LOG(ERROR) << "Failed to handle event: " << errno;
        return -1;
    }

    for (int i = 0; i < event_ret; i++) {
    #ifdef __linux__
        fd = events[i].data.fd;
    #elif __APPLE__
        fd = events[i].ident;
    #endif
        if (fd == tcp_srv_fd) {
            if (accept_new_client() < 0) ret = -1;
            continue;
        }
    #ifdef __linux__
        handle_client_event(fd, events[i].events);
    #elif __APPLE__
        const uint32_t err_mask = EV_ERROR;
        if (events[i].flags & err_mask) {
            close_and_delete_event(fd, EVFILT_READ);
            return -1;
        }
        handle_client_event(fd, events[i].filter);
    #endif
    }
}

}