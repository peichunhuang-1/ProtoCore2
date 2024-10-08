#ifndef RATE_H
#define RATE_H

#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <glog/logging.h>

#ifdef __linux__
// Linux-specific includes
#include <sys/timerfd.h>
#include <sys/epoll.h>
#elif __APPLE__
// macOS-specific includes
#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>
#else
#error "Unsupported system"
#endif

namespace core {
    class Rate {
    public:
        Rate(float freq) {
#ifdef __linux__
            // Linux-specific initialization
            timer_fd = timerfd_create(CLOCK_MONOTONIC, 0);
            if (timer_fd == -1) {
                LOG(ERROR) << "Failed to create timer file descriptor!";
                return;
            }
            interval_ns = static_cast<long>(1e9 / freq);

            struct itimerspec interval;
            interval.it_interval.tv_sec = interval_ns / 1000000000;
            interval.it_interval.tv_nsec = interval_ns % 1000000000;
            interval.it_value.tv_sec = interval.it_interval.tv_sec;
            interval.it_value.tv_nsec = interval.it_interval.tv_nsec;

            if (timerfd_settime(timer_fd, 0, &interval, NULL) == -1) {
                LOG(ERROR) << "Failed to set timer file descriptor: " << errno;
                close(timer_fd);
                return;
            }
            epoll_fd = epoll_create(1);
            if (epoll_fd == -1) {
                LOG(ERROR) << "Failed to create epoll file descriptor: " << errno;
                close(timer_fd);
                return;
            }
            ev.events = EPOLLIN;
            ev.data.fd = timer_fd;
            if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, timer_fd, &ev) == -1) {
                LOG(ERROR) << "Failed to add epoll event: " << errno;
                close(epoll_fd);
                close(timer_fd);
                return;
            }
#elif __APPLE__
            // macOS-specific initialization
            kq = kqueue();
            if (kq == -1) {
                LOG(ERROR) << "Failed to create kqueue!";
                return;
            }

            struct kevent change;
            EV_SET(&change, 1, EVFILT_TIMER, EV_ADD | EV_ENABLE, NOTE_NSECONDS, static_cast<int>(1e9 / freq), 0);
            timeout.tv_sec = 0;
            timeout.tv_nsec = 0;
            struct kevent event;
            int nev = kevent(kq, &change, 1, &event, 1, NULL);
            if (nev == -1) {
                LOG(ERROR) << "Failed to initialize timer!";
                close(kq);
                return;
            }
#endif
        }

        bool sleep() {
#ifdef __linux__
            // Linux-specific sleep implementation
            struct epoll_event events;
            int nfds = epoll_wait(epoll_fd, &events, 1, -1);
            if (nfds == -1) {
                LOG(ERROR) << "Epoll wait error: " << errno;
                return false;
            }
            if (events.data.fd == timer_fd) {
                uint64_t expirations;
                ssize_t s = read(timer_fd, &expirations, sizeof(expirations));
                if (s != sizeof(expirations)) {
                    LOG(ERROR) << "Failed to read timer expirations: " << errno;
                    return false;
                }
                return expirations == 1;
            }
            return false;
#elif __APPLE__
            // macOS-specific sleep implementation
            struct kevent event;
            int nev = kevent(kq, NULL, 0, &event, 1, NULL);
            if (nev == -1) {
                LOG(ERROR) << "Failed to wait for timer event: " << errno;
                return false;
            }
            return nev == 1;
#endif
        }

        bool vtask_loop() { return sleep(); }

        template<typename function, typename... functions>
        bool vtask_loop(function&& func, functions&&... funcs) {
#ifdef __linux__
            // Linux-specific vtask_loop implementation
            struct epoll_event events;
            int nfds = epoll_wait(epoll_fd, &events, 1, 0);
            if (nfds == -1) {
                LOG(ERROR) << "Epoll wait error: " << errno;
                return false;
            }
            if (nfds == 0) {
                func();
                return vtask_loop(std::forward<functions>(funcs)...);
            } else if (events.data.fd == timer_fd) {
                uint64_t expirations;
                ssize_t s = read(timer_fd, &expirations, sizeof(expirations));
                if (s != sizeof(expirations)) {
                    LOG(ERROR) << "Failed to read timer expirations: " << errno;
                    return false;
                }
                return expirations == 1;
            }
            return false;
#elif __APPLE__
            // macOS-specific vtask_loop implementation
            struct kevent event;
            int nev = kevent(kq, NULL, 0, &event, 1, &timeout);
            if (nev == 0) {
                func();
                return vtask_loop(std::forward<functions>(funcs)...);
            } else if (nev == -1) {
                LOG(ERROR) << "Failed to wait for timer event: " << errno;
                return false;
            } else {
                return nev == 1;
            }
#endif
        }

        ~Rate() {
#ifdef __linux__
            // Linux-specific cleanup
            close(timer_fd);
            close(epoll_fd);
#elif __APPLE__
            // macOS-specific cleanup
            close(kq);
#endif
        }

    private:
#ifdef __linux__
        int timer_fd;
        long interval_ns;
        int epoll_fd;
        struct epoll_event ev;
#elif __APPLE__
        int kq;
        struct timespec timeout;
#endif
    };
}
#endif