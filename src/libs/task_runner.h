#ifndef __TASK_RUNNER_H__
#define __TASK_RUNNER_H__

#include <functional>
#include <deque>
#include <vector>
#include <stdexcept>
#include <iterator>
#include <mutex>

#include "util.h"

template <typename Fn>
class TaskRunner {
    private:
        struct Task {
            bool once;
            std::function<Fn> func;
        };
        std::vector<std::deque<Task>> tasks;
        mutable std::mutex mtx;
    public:
        ~TaskRunner() = default;
        
        void push_oncef(const unsigned int which, const std::function<Fn>& func);
        void push_onceb(const unsigned int which, const std::function<Fn>& func);

        void pushf(const unsigned int which, const std::function<Fn>& func);
        void pushb(const unsigned int which, const std::function<Fn>& func);

        void popf(const unsigned int which);
        void popb(const unsigned int which);

        void new_session(const unsigned int cnt);
        
        void run();
    private:
        std::deque<Task>& session_at(unsigned int idx);
        void _pushb(std::deque<Task>& session, bool flag, const std::function<Fn>& func);
        void _pushf(std::deque<Task>& session, bool flag, const std::function<Fn>& func);

        template <typename Op>
        void exec_locked(unsigned int idx, Op&& op);
};

// It is recommended to use universal ref-perfect forwarding when making wrapper.
template <typename Callable>
auto AsThrottle(Callable&& func, msec64 timeout) {
    return [func = std::forward<Callable>(func), timeout, last = msec64(0)]() mutable {
        msec64 now = now_ms();
        if (now - last < timeout) return;
        last = now;
        func();
    };
}

#include "task_runner.tpp"
#endif
