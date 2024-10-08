#ifndef ASYNC_TCP_H
#define ASYNC_TCP_H
#ifdef __linux__
#include <sys/epoll.h>
#elif __APPLE__
#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>
#include <netinet/tcp.h>
#else
#error "Unsupported system"
#endif
#include <fcntl.h>
#include <glog/logging.h>
#include <shared_mutex>

namespace core {
    using namespace std;
    class Socket {
        public:
        static const int                maxevents = 128;
        int                             set_nonblock(const int& fd);
        int                             create_socket();
    #ifdef __linux__
        int                             epoll_fd;
        struct epoll_event              events[maxevents];
        int                             add_epoll_event(const int& fd, const int& events); 
        int                             delete_epoll_event(const int& fd); 
        int                             init_epoll(); 
    #elif __APPLE__
        int                             kq_fd;
        struct kevent                   events[maxevents];
        int                             add_kqueue_event(const int& fd, const int& filter, const int& flags); 
        int                             delete_kqueue_event(const int& fd, const int& filter); 
        int                             init_kqueue(); 
    #endif
    };
}

#endif