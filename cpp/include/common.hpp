#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <errno.h>
#include <condition_variable>
#include <shared_mutex>

#define DEFAULT_CORE_MASTER_ADR "127.0.0.1:50051"
#define DEFAULT_CORE_LOCAL_IP   "127.0.0.1"


inline std::atomic<bool> __CORE__OK__(true);
inline std::atomic<bool> __USE__SIM__TIME__(false);

namespace core {
    inline bool ok() {
        return __CORE__OK__.load();
    }
    inline void setAbort() {
        __CORE__OK__ = false;
    }
}

#endif