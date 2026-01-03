#ifndef __TASK_RUNNER_H__
#define __TASK_RUNNER_H__

#include <functional>
#include <deque>
#include <vector>
#include <stdexcept>
#include <iterator>
#include <mutex>

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
};

#include "task_runner.tpp"
#endif
