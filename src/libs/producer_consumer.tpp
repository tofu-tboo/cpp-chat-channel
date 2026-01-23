
#include "producer_consumer.h"

template <typename T>
ProducerConsumerQueue<T>::~ProducerConsumerQueue() {
	stop();
	while (!queue_.empty()) {
		queue_.pop();
	}
}

template <typename T>
void ProducerConsumerQueue<T>::push(T item) {
	{
		std::lock_guard<std::mutex> lock(mutex_);
		queue_.push(std::move(item));
	}
	cond_.notify_one();
}

template <typename T>
bool ProducerConsumerQueue<T>::wait_and_pop(T& out_item) {
	std::unique_lock<std::mutex> lock(mutex_);
	cond_.wait(lock, [this]() { return stopped_ || !queue_.empty(); });
	if (stopped_ && queue_.empty()) {
		return false;
	}
	out_item = std::move(queue_.front());
	queue_.pop();
	return true;
}

template <typename T>
bool ProducerConsumerQueue<T>::try_pop(T& out_item) {
	std::lock_guard<std::mutex> lock(mutex_);
	if (queue_.empty()) {
		return false;
	}
	out_item = std::move(queue_.front());
	queue_.pop();
	return true;
}

template <typename T>
std::queue<T> ProducerConsumerQueue<T>::pop_all() {
	std::lock_guard<std::mutex> lock(mutex_);
	std::queue<T> local_q;
	std::swap(queue_, local_q);
	return local_q;
}

template <typename T>
bool ProducerConsumerQueue<T>::empty() const {
	std::lock_guard<std::mutex> lock(mutex_);
	return queue_.empty();
}

template <typename T>
size_t ProducerConsumerQueue<T>::size() const {
	std::lock_guard<std::mutex> lock(mutex_);
	return queue_.size();
}

template <typename T>
void ProducerConsumerQueue<T>::stop() {
	{
		std::lock_guard<std::mutex> lock(mutex_);
		stopped_ = true;
	}
	cond_.notify_all();
}