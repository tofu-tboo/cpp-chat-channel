#include "signal_handler_thread.h"
#include <vector>

SignalHandler::SignalHandler() : m_running(false) {
    // 기본적으로 처리할 시그널들: Ctrl+C(SIGINT), 종료(SIGTERM)
    sigemptyset(&m_signal_set);
    sigaddset(&m_signal_set, SIGINT);
    sigaddset(&m_signal_set, SIGTERM);
}

SignalHandler::~SignalHandler() {
    stop();
}

void SignalHandler::setup_mask() {
    pthread_sigmask(SIG_BLOCK, &m_signal_set, nullptr);
}

void SignalHandler::start(Callback callback) {
    m_running = true;
    m_handler_thread = std::thread([this, callback]() {
        int sig;
        while (m_running) {
            // 시그널이 올 때까지 여기서 대기 (CPU를 점유하지 않음)
            if (sigwait(&m_signal_set, &sig) == 0) {
                if (!m_running) break;
                callback(sig); // 안전하게 사용자 콜백 실행
            }
        }
    });
}

void SignalHandler::stop() {
    if (m_running) {
        m_running = false;
        // 스레드가 sigwait에서 깨어나도록 시그널을 자신에게 보냄
        pthread_kill(m_handler_thread.native_handle(), SIGTERM);
        if (m_handler_thread.joinable()) {
            m_handler_thread.join();
        }
    }
}

