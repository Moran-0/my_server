#include "CurrentThread.h"
#include <sys/syscall.h>
#include <unistd.h>

namespace CurrentThread {
    __thread int t_cachedTid = 0;
    __thread char t_tidString[32];
    __thread int t_tidStringLength = 6;
    __thread const char* t_threadName = "unknown";

    void CacheTid() {
        t_cachedTid = static_cast<int>(GetTid());
        t_tidStringLength = snprintf(t_tidString, sizeof(t_tidString), "%5d ", t_cachedTid);
    }
    // 通过系统调用获取线程ID,不使用std::this_thread::get_id()是因为返回值仅进程内唯一，不保证全局唯一
    pid_t GetTid() {
        return static_cast<pid_t>(::syscall(SYS_gettid));
    }
}