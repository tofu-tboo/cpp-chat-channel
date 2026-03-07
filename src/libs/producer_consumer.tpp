
#include "producer_consumer.h"

template <typename T>
ProducerConsumerQueue<T>::~ProducerConsumerQueue() {
	stop();
	
    // Cleanup atomic stack
    PCQNode<T>* current = head_.load();
    while (current) {
        PCQNode<T>* next = current->next;
        delete current;
        current = next;
    }
    // Cleanup cache
    consumer_cache_.clear();
}

template <typename T>
void ProducerConsumerQueue<T>::push(T item) {
	PCQNode<T>* node = new PCQNode<T>{std::move(item), nullptr};
    
    // Lock-free push to head (LIFO)
    node->next = head_.load(std::memory_order_relaxed);
    while (!head_.compare_exchange_weak(node->next, node, std::memory_order_release, std::memory_order_relaxed));

    // Signal if not already signaled
    if (!signaled_.exchange(true, std::memory_order_release)) {
        sem_.release();
    }
}

template <typename T>
bool ProducerConsumerQueue<T>::wait_and_pop(T& out_item) {
    std::unique_lock lock(consumer_mtx_);
    while (true) {
        if (!consumer_cache_.empty()) {
            out_item = std::move(consumer_cache_.front());
            consumer_cache_.pop_front();
            return true;
        }

        if (stopped_) return false;

        lock.unlock();
        sem_.acquire();
        lock.lock();

        if (stopped_ && consumer_cache_.empty() && !head_.load(std::memory_order_relaxed)) return false;

        signaled_.store(false, std::memory_order_release);
        PCQNode<T>* local_head = head_.exchange(nullptr, std::memory_order_acquire);
        
        // Reverse LIFO to FIFO and fill cache
        while (local_head) {
            PCQNode<T>* next = local_head->next;
            consumer_cache_.push_front(std::move(local_head->data)); // push_front to reverse
            delete local_head;
            local_head = next;
        }
    }
}

template <typename T>
bool ProducerConsumerQueue<T>::wait_and_pop_all(std::deque<T>& out_item) {
	std::unique_lock lock(consumer_mtx_);
    while (true) {
        if (!consumer_cache_.empty()) {
            // If we have cached items, we also check head to grab everything in one go if possible
            // But strictly, just returning cache is enough.
            // To be truly "pop_all", we should try to grab head too.
            signaled_.store(false, std::memory_order_release);
            PCQNode<T>* local_head = head_.exchange(nullptr, std::memory_order_acquire);
            while (local_head) {
                PCQNode<T>* next = local_head->next;
                consumer_cache_.push_front(std::move(local_head->data));
                delete local_head;
                local_head = next;
            }
            
            std::swap(out_item, consumer_cache_);
            return true;
        }

        if (stopped_) return false;

        lock.unlock();
        sem_.acquire();
        lock.lock();

        if (stopped_ && consumer_cache_.empty() && !head_.load(std::memory_order_relaxed)) return false;

        // Fetch from head (same logic as above, but loop ensures we retry if spurious wake)
        signaled_.store(false, std::memory_order_release);
        PCQNode<T>* local_head = head_.exchange(nullptr, std::memory_order_acquire);
        while (local_head) {
            PCQNode<T>* next = local_head->next;
            consumer_cache_.push_front(std::move(local_head->data));
            delete local_head;
            local_head = next;
        }
	}
}

template <typename T>
bool ProducerConsumerQueue<T>::try_pop(T& out_item) {
	std::lock_guard lock(consumer_mtx_);
    if (consumer_cache_.empty()) {
        // Try to fetch from head
        signaled_.store(false, std::memory_order_release);
        PCQNode<T>* local_head = head_.exchange(nullptr, std::memory_order_acquire);
        if (!local_head) return false;

        while (local_head) {
            PCQNode<T>* next = local_head->next;
            consumer_cache_.push_front(std::move(local_head->data));
            delete local_head;
            local_head = next;
        }
    }
    
    out_item = std::move(consumer_cache_.front());
    consumer_cache_.pop_front();
    return true;
}

template <typename T>
std::deque<T> ProducerConsumerQueue<T>::pop_all() {
	std::deque<T> out_item;
    // Reuse wait_and_pop_all logic but non-blocking? 
    // Actually pop_all usually implies non-blocking drain.
    std::lock_guard lock(consumer_mtx_);
    
    // Fetch everything
    signaled_.store(false, std::memory_order_release);
    PCQNode<T>* local_head = head_.exchange(nullptr, std::memory_order_acquire);
    while (local_head) {
        PCQNode<T>* next = local_head->next;
        consumer_cache_.push_front(std::move(local_head->data));
        delete local_head;
        local_head = next;
    }
    
    std::swap(out_item, consumer_cache_);
    return out_item;
}

template <typename T>
bool ProducerConsumerQueue<T>::empty() const {
	std::lock_guard lock(consumer_mtx_);
	return consumer_cache_.empty() && !head_.load(std::memory_order_relaxed);
}

template <typename T>
size_t ProducerConsumerQueue<T>::size() const {
	std::lock_guard lock(consumer_mtx_);
	return consumer_cache_.size(); // Approximate
}

template <typename T>
void ProducerConsumerQueue<T>::stop() {
    stopped_ = true;
    sem_.release(); // Wake up any waiter
}