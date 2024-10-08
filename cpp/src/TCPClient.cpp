#include "TCPClient.hpp"
#include "rscl.hpp"
namespace core {
TCPClient::TCPClient() {
    clients_data = vector<string>(100, "");
    LOG(INFO) << "Create client club";
}

void TCPClient::close_and_delete_event(const int& fd) {
    close(fd);
    return;
}
bool TCPClient::get_socket_info(const int& fd, string& src_ip, int& src_port) {
    struct sockaddr_in local_addr;
    socklen_t addr_len = sizeof(local_addr);
    if (getsockname(fd, (struct sockaddr*)&local_addr, &addr_len) == -1) {
        close_and_delete_event(fd);
        return false;
    }

    char src_ip_buf[15];
    src_port = ntohs(local_addr.sin_port);
    src_ip = string(inet_ntop(AF_INET, &local_addr.sin_addr, src_ip_buf, sizeof(src_ip_buf)));
    if (!src_ip.data()) {
        close_and_delete_event(fd);
        return false;
    }
    return true;
}

bool TCPClient::write_to_cache(const string& topic, const string& data) {
    unique_lock<shared_mutex> lock(mtx);
    return static_topic_cache[topic].insert(data).second;
}

client_info TCPClient::add_client(const string& topic, const string& ip, const int& port) {
    unique_lock<shared_mutex> lock(mtx);
    int fd = create_socket();
    if (fd < 0) return client_info{};
    struct sockaddr_in dest;
    bzero(&dest, sizeof(dest));
    dest.sin_family = AF_INET;
    dest.sin_port = htons(port);
    promise<bool> promise;
    client_info info;
    info.connected = promise.get_future();
    if ( inet_pton(AF_INET, ip.c_str(), &dest.sin_addr.s_addr) == 0 ) {
        close_and_delete_event(fd);
        promise.set_value(false);
        return info;
    }
    if (connect(fd, (struct sockaddr*)&dest, sizeof(dest)) < 0 ) {
        if(errno != EINPROGRESS) {
            close_and_delete_event(fd);
            promise.set_value(false);
            return info;
        } else {
            string src_ip;
            int src_port;
            if (!get_socket_info(fd, src_ip, src_port)) {
                close_and_delete_event(fd);
                promise.set_value(false);
                return info;
            } else {
                info.ip = src_ip;
                info.port = src_port;
            }
            thread([fd, promise = std::move(promise), this, topic]() mutable {
                fd_set write_fds;
                FD_ZERO(&write_fds);
                FD_SET(fd, &write_fds);
                int err;
                socklen_t len = sizeof(err);
                if (select(fd + 1, nullptr, &write_fds, nullptr, nullptr) > 0) {
                    if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &len) < 0) promise.set_value(false);
                    else {
                        if (err == 0) {
                            clients_topic_fd[topic].insert(fd);
                            if (clients_data.size() <= fd) clients_data.resize(clients_data.size() + 100);
                            string data_init = "";
                            if (static_topic_cache.count(topic)) {
                                for (auto s: static_topic_cache[topic]) {
                                    data_init.append(s.data(), s.length());
                                }
                            }
                            clients_data[fd] = data_init;
                            promise.set_value(true);
                            unique_lock<shared_mutex> lock(this->mtx);
                            this->write_fd(topic, fd);
                        } else {
                            close_and_delete_event(fd);
                            promise.set_value(false);
                        }
                    }
                } else {
                    close_and_delete_event(fd);
                    promise.set_value(false);
                }
            }).detach();
        }
    }
    return info;
}

void TCPClient::write_to_socket(const string& topic, const string& msg, const int& timeout) {
    unique_lock<shared_mutex> lock(mtx);
    for (auto fd: clients_topic_fd[topic]) {
        if (clients_data[fd].length() > MAX_WRITE_BUFFER_SIZE) continue;
        clients_data[fd].append(msg.data(), msg.length());
        write_fd(topic, fd);
    }
}

void TCPClient::write_fd(const string& topic, const int& fd) {
    const string to_send = clients_data[fd];
    int n = to_send.length(); int siz = n;
    int nwrite = 0;
    while (n > 0) {
        nwrite = write(fd, to_send.data() + siz - n, n);
        if (nwrite < n) {
            if (nwrite == -1 && errno != EAGAIN) {
                LOG(ERROR) << "Failed to write to socket";
                clients_topic_fd[topic].erase(fd);
                clients_data[fd] = "";
                close_and_delete_event(fd);
                return;
            }
            else if (errno == EAGAIN) break;
        }
        n -= nwrite;
    }
    clients_data[fd].erase(0, siz - n);
}
}