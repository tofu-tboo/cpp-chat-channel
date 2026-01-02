#ifndef __TASK_RUNNER_H__
#define __TASK_RUNNER_H__

#include <functional>
#include <deque>

template <typename Fn>
class TaskRunner {
    private:
        struct Task {
            bool once;
            std::function<Fn> func;
        };
        std::deque<Task> tasks;
    public:
        ~TaskRunner();
        
        void push_oncef(const std::function<Fn>& func);
        void push_onceb(const std::function<Fn>& func);

        void pushf(const std::function<Fn>& func);
        void pushb(const std::function<Fn>& func);

        void popf();
        void popb();
        
        void run();
    private:
        void _pushb(bool flag, const std::function<Fn>& func);
        void _pushf(bool flag, const std::function<Fn>& func);
};

#include "task_runner.tpp"
#endif