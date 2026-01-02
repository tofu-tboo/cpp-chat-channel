
#include "task_runner.h"


template <typename Fn>
TaskRunner<Fn>::~TaskRunner() {
        tasks.clear();
}

template <typename Fn>
void TaskRunner<Fn>::push_oncef(const std::function<Fn>& func) {
        _pushf(true, func);
}
template <typename Fn>
void TaskRunner<Fn>::push_onceb(const std::function<Fn>& func) {
        _pushb(true, func);
}

template <typename Fn>
void TaskRunner<Fn>::pushf(const std::function<Fn>& func) {
        _pushf(false, func);
}
template <typename Fn>
void TaskRunner<Fn>::pushb(const std::function<Fn>& func) {
        _pushb(false, func);
}

template <typename Fn>
void TaskRunner<Fn>::popf() {
        tasks.pop_front();
}
template <typename Fn>
void TaskRunner<Fn>::popb() {
        tasks.pop_back();
}

template <typename Fn>
void TaskRunner<Fn>::run() {
        for (auto it = tasks.begin(); it != tasks.end();) {
                it->func();
                if (it->once) {
                        it = tasks.erase(it);
                } else {
                        it++;
                }
        }
}

#pragma region PRIVATE_FUNC
template <typename Fn>
void TaskRunner<Fn>::_pushb(bool flag, const std::function<Fn>& func) {
        tasks.push_back({flag, func});
}
template <typename Fn>
void TaskRunner<Fn>::_pushf(bool flag, const std::function<Fn>& func) {
        tasks.push_front({flag, func});
}
#pragma endregion