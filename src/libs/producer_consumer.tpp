
#include "producer_consumer.h"

template <typename T>
ProducerConsumerQueue<T>::~ProducerConsumerQueue() {
	stop();
	while (!queue_.empty()) {
		queue_.pop_front();
	}
}

template <typename T>
void ProducerConsumerQueue<T>::push(T item) {
	{
		std::lock_guard lock(mutex_);
		queue_.push_back(std::move(item));
	}
	cond_.notify_one();
}

template <typename T>
bool ProducerConsumerQueue<T>::wait_and_pop(T& out_item) {
	std::unique_lock lock(mutex_);
    cond_.wait(lock, [this] { return stopped_ || !queue_.empty(); }); // spurious wakeup 방지
    if (stopped_ || queue_.empty()) return false; // 즉시 반환
    out_item = std::move(queue_.front());
    queue_.pop_front();
    return true;
}

template <typename T>
bool ProducerConsumerQueue<T>::wait_and_pop_all(std::deque<T>& out_item) {
	std::unique_lock lock(mutex_);
    cond_.wait(lock, [this] { return stopped_ || !queue_.empty(); });
    if (stopped_ || queue_.empty()) return false;
    std::swap(queue_, out_item);
    return true;
}

template <typename T>
bool ProducerConsumerQueue<T>::try_pop(T& out_item) {
	std::lock_guard lock(mutex_);
	if (queue_.empty()) {
		return false;
	}
	out_item = std::move(queue_.front());
	queue_.pop_front();
	return true;
}

template <typename T>
std::deque<T> ProducerConsumerQueue<T>::pop_all() {
	std::lock_guard lock(mutex_);
	std::deque<T> local_q;
	std::swap(queue_, local_q);
	return local_q;
}

template <typename T>
bool ProducerConsumerQueue<T>::empty() const {
	std::lock_guard lock(mutex_);
	return queue_.empty();
}

template <typename T>
size_t ProducerConsumerQueue<T>::size() const {
	std::lock_guard lock(mutex_);
	return queue_.size();
}

template <typename T>
void ProducerConsumerQueue<T>::stop() {
	{
		std::lock_guard lock(mutex_);
		stopped_ = true;
	}
	cond_.notify_all();
}