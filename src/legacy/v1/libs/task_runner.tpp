#include "task_runner.h"
#include "util.h"
#include <chrono>

template <typename Fn>
template <typename Op>
void TaskRunner<Fn>::exec_locked(unsigned int idx, Op&& op) {
    std::lock_guard lock(mtx);
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
    std::lock_guard lock(mtx);
    for (unsigned int i = 0; i < cnt; i++) {
        tasks.emplace_back();
    }
}

template <typename Fn>
void TaskRunner<Fn>::clear() {
    std::lock_guard lock(mtx);
    tasks.clear();
}

template <typename Fn>
void TaskRunner<Fn>::run() {
    std::lock_guard lock(mtx);
    for (size_t i = 0; i < tasks.size(); ++i) {
        auto& session = tasks[i];
        for (auto it = session.begin(); it != session.end();) {
#ifdef DEBUG
            auto start = std::chrono::high_resolution_clock::now();
#endif
            it->func();
#ifdef DEBUG
            auto end = std::chrono::high_resolution_clock::now();
            auto dur = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
            if (dur > 1000) DLOG(_CY_ "[TaskRunner] Session %d task took %lld us" _EC_, (int)i, (long long)dur);
#endif
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

template <typename Callable>
auto AsThrottle(Callable&& func, msec64 timeout)  {
    return [func = std::forward<Callable>(func), timeout, last = msec64(0)]() mutable {
        msec64 now = now_ms();
        if (now - last < timeout) return;
        last = now;
        func();
    };
}