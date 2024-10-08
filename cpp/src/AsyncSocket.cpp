#include "AsyncSocket.hpp"
namespace core {
        /*
    Following is the function associate with epoll or kqueue. Determined by your OS, as for MacOS we use kqueue, for linux we use epoll.
    We have only three function, init, add and delete event.
    */
    int Socket::set_nonblock(const int& fd) {
        int flags = fcntl(fd, F_GETFL, 0);
        if (flags == -1) {
            LOG(ERROR) << "Failed to get file flags: " << errno;
            return -1;
        }
        if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
            LOG(ERROR) << "Failed to set non-blocking mode: " << errno;
            return -1;
        }
        return 0;
    }
    int Socket::create_socket() {
        int sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (sockfd < 0) {
            LOG(ERROR) << "Failed to create socket: " << errno;
        } else {
            if (set_nonblock(sockfd) < 0) return -1;
        }
        return sockfd;
    }

    #ifdef __linux__
    int Socket::add_epoll_event(const int& fd, const int& events) {
        struct epoll_event event;
        memset(&event, 0, sizeof(struct epoll_event)); // prevent undefine behavior

        event.events  = events;
        event.data.fd = fd;

        if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &event) < 0) {
            LOG(ERROR) << "Failed to add epoll event: " << errno;
            return -1;
        }
        return 0;
    }
    int Socket::delete_epoll_event(const int& fd) {

        if (epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, NULL) < 0) {
            LOG(ERROR) << "Failed to delete epoll event: " << errno;
            return -1;
        }
        return 0;
    }
    int Socket::init_epoll() {
        epoll_fd = epoll_create(maxevents);
        if (epoll_fd < 0) {
            LOG(ERROR) << "Failed to init epoll: " << errno;
            return -1;
        }
        return 0;
    }
    #elif __APPLE__
    int Socket::add_kqueue_event(const int& fd, const int& filter, const int& flags) {
        struct kevent event;

        EV_SET(&event, fd, filter, flags, 0, 0, NULL);
        if (kevent(kq_fd, &event, 1, NULL, 0, NULL) < 0) {
            LOG(ERROR) << "Failed to add kqueue event: " << errno;
            return -1;
        }
        return 0;
    }
    int Socket::delete_kqueue_event(const int& fd, const int& filter) {
        struct kevent event;

        EV_SET(&event, fd, filter, EV_DELETE, 0, 0, NULL);
        if (kevent(kq_fd, &event, 1, NULL, 0, NULL) < 0) {
            LOG(ERROR) << "Failed to delete kqueue event: " << errno;
            return -1;
        }
        return 0;
    }
    int Socket::init_kqueue() {
        kq_fd = kqueue();
        if (kq_fd < 0) {
            LOG(ERROR) << "Failed to init kqueue: " << errno;
            return -1;
        }
        return 0;
    }
    #endif
}