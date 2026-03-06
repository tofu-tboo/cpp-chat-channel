#ifndef __SIGNAL_HANDLER_THREAD_H__
#define __SIGNAL_HANDLER_THREAD_H__

#include <csignal>
#include <functional>
#include <thread>
#include <atomic>

class SignalHandler {
public:
    using Callback = std::function<void(int)>;

    SignalHandler();

    ~SignalHandler();

    // 시그널 마스크 설정 (메인 스레드 시작 시 호출 필요)
    void setup_mask();

    // 시그널 대기 스레드 시작
    void start(Callback callback);

    void stop();

private:
    sigset_t m_signal_set;
    std::thread m_handler_thread;
    std::atomic<bool> m_running;
};

#endif