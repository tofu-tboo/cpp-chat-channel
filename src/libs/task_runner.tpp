#include "task_runner.h"

template <typename Fn>
template <typename Op>
void TaskRunner<Fn>::exec_locked(unsigned int idx, Op&& op) {
    std::lock_guard<std::mutex> lock(mtx);
    op(session_at(idx));
}

template <typename Fn>
void TaskRunner<Fn>::push_oncef(const unsigned int which, const std::function<Fn>& func) {
    exec_locked(which, [&](auto& session) { _pushf(session, true, func); });
}
template <typename Fn>
void TaskRunner<Fn>::push_onceb(const unsigned int which, const std::function<Fn>& func) {
    exec_locked(which, [&](auto& session) { _pushb(session, true, func); });
}

template <typename Fn>
void TaskRunner<Fn>::pushf(const unsigned int which, const std::function<Fn>& func) {
    exec_locked(which, [&](auto& session) { _pushf(session, false, func); });
}
template <typename Fn>
void TaskRunner<Fn>::pushb(const unsigned int which, const std::function<Fn>& func) {
    exec_locked(which, [&](auto& session) { _pushb(session, false, func); });
}

template <typename Fn>
void TaskRunner<Fn>::popf(const unsigned int which) {
    exec_locked(which, [](auto& session) { session.pop_front(); });
}
template <typename Fn>
void TaskRunner<Fn>::popb(const unsigned int which) {
    exec_locked(which, [](auto& session) { session.pop_back(); });
}

template <typename Fn>
void TaskRunner<Fn>::new_session(const unsigned int cnt) {
    std::lock_guard<std::mutex> lock(mtx);
    for (unsigned int i = 0; i < cnt; i++) {
        tasks.emplace_back();
    }
}

template <typename Fn>
void TaskRunner<Fn>::run() {
    std::lock_guard<std::mutex> lock(mtx);
    for (auto& session : tasks) {
        for (auto it = session.begin(); it != session.end();) {
            it->func();
            it = it->once ? session.erase(it) : std::next(it);
        }
    }
}

#pragma region PRIVATE_FUNC
template <typename Fn>
std::deque<typename TaskRunner<Fn>::Task>& TaskRunner<Fn>::session_at(unsigned int idx) {
    if (idx >= tasks.size()) {
        throw std::runtime_error("Task session out of range.");
    }
    return tasks[idx];
}
template <typename Fn>
void TaskRunner<Fn>::_pushb(std::deque<Task>& session, bool flag, const std::function<Fn>& func) {
    session.push_back({flag, func});
}
template <typename Fn>
void TaskRunner<Fn>::_pushf(std::deque<Task>& session, bool flag, const std::function<Fn>& func) {
    session.push_front({flag, func});
}
