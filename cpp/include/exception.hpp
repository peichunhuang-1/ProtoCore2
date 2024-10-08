#ifndef EXCEPTION_H
#define EXCEPTION_H
#include <stdio.h>
#include <errno.h>
#include <csignal>
#include <glog/logging.h>
#include "common.hpp"

namespace core_exception {
    inline std::once_flag __exception_catcher_call_flag__;
    inline void signalInterruptHandler( int signum ) {
        __CORE__OK__ = false;
        LOG(ERROR) << "SIGINT: exit from process";
        google::FlushLogFiles(google::GLOG_INFO);
        exit(signum);
    }
    inline void signalPipeHandler( int signum ) {
        LOG(WARNING) << "Warning: ignore SIGPIPE";
    }
    inline void catcher_init() {
        std::call_once(__exception_catcher_call_flag__, []() {
            signal(SIGPIPE, signalPipeHandler);
            signal(SIGINT, signalInterruptHandler);
        });
    }
}


#endif