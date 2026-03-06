#include "auto_lock_container.h"
#include <algorithm>
#include <optional>

// 1. Add
template <class STL>
void AutoLockContainer<STL>::add(const value_type& item) requires (ALC_Traits::HasPushBack<STL> || ALC_Traits::HasPush<STL> || (ALC_Traits::HasKey<STL> && !ALC_Traits::HasMapped<STL>)) {
    std::lock_guard lock(mtx);
    if constexpr (ALC_Traits::HasPushBack<STL>) container.push_back(item);
    else if constexpr (ALC_Traits::HasPush<STL>) container.push(item);
    else container.insert(item);
}

template <class STL>
template <typename Key, typename Value>
void AutoLockContainer<STL>::add(const Key& key, const Value& value) requires ALC_Traits::HasMapped<STL> && std::convertible_to<Value, typename STL::mapped_type> {
    std::lock_guard lock(mtx);
    if constexpr (requires { container[key] = value; }) container[key] = value;
    else container.insert({key, value});
}

template <class STL>
template <typename Key, typename SubValue>
void AutoLockContainer<STL>::add(const Key& key, const SubValue& subval) 
    requires ALC_Traits::HasMapped<STL> 
            && (ALC_Traits::MappedHasPush<STL> || ALC_Traits::MappedHasPushBack<STL> || ALC_Traits::MappedHasInsert<STL>)
            && (!std::convertible_to<SubValue, typename STL::mapped_type>) 
{
    std::lock_guard lock(mtx);
    if constexpr (ALC_Traits::MappedHasPush<STL>) container[key].push(subval);
    else if constexpr (ALC_Traits::MappedHasPushBack<STL>) container[key].push_back(subval);
    else container[key].insert(subval);
}

// 2. Delete
template <class STL>
template <typename Key>
bool AutoLockContainer<STL>::del(const Key& key) requires ALC_Traits::HasKey<STL> {
    std::lock_guard lock(mtx);
    return container.erase(key) > 0;
}

template <class STL>
bool AutoLockContainer<STL>::del(value_type& out_item) requires (ALC_Traits::HasFront<STL> || ALC_Traits::HasTop<STL>) {
    std::lock_guard lock(mtx);
    if (container.empty()) return false;
    
    if constexpr (ALC_Traits::HasFront<STL>) out_item = container.front();
    else out_item = container.top();
    
    container.pop();
    return true;
}

// 4. Read
template <class STL>
template <typename Key>
bool AutoLockContainer<STL>::get(const Key& key, typename STL::mapped_type& out_value) const requires ALC_Traits::HasMapped<STL> {
    std::shared_lock lock(mtx);
    auto it = container.find(key);
    if (it != container.end()) {
        out_value = it->second;
        return true;
    }
    return false;
}

template <class STL>
bool AutoLockContainer<STL>::get(value_type& out_item) const requires (ALC_Traits::HasFront<STL> || ALC_Traits::HasTop<STL>) {
    std::shared_lock lock(mtx);
    if (container.empty()) return false;
    
    if constexpr (ALC_Traits::HasFront<STL>) out_item = container.front();
    else out_item = container.top();
    
    return true;
}


// 5. Utilities
template <class STL>
size_t AutoLockContainer<STL>::size() const {
    std::shared_lock lock(mtx);
    return container.size();
}

template <class STL>
bool AutoLockContainer<STL>::empty() const {
    std::shared_lock lock(mtx);
    return container.empty();
}

template <class STL>
void AutoLockContainer<STL>::clear() requires ALC_Traits::HasClear<STL> {
    std::lock_guard lock(mtx);
    container.clear();
}

template <class STL>
void AutoLockContainer<STL>::clear() requires (!ALC_Traits::HasClear<STL>) {
    std::lock_guard lock(mtx);
    container = STL(); // Re-assign empty container for adapters (queue, stack)
}

template <class STL>
template <typename Key>
bool AutoLockContainer<STL>::find(const Key& key) const requires ALC_Traits::HasKey<STL> {
    std::shared_lock lock(mtx);
    return container.find(key) != container.end();
}

template <class STL>
bool AutoLockContainer<STL>::find(const value_type& item) const requires (!ALC_Traits::HasKey<STL> && ALC_Traits::HasIterators<STL>) {
    std::shared_lock lock(mtx);
    return std::find(container.begin(), container.end(), item) != container.end();
}

// 6. Task
template <class STL>
template <typename Func>
void AutoLockContainer<STL>::task(Func&& func) {
    std::lock_guard lock(mtx);
    func(container);
}

template <class STL>
STL AutoLockContainer<STL>::move() {
    std::lock_guard lock(mtx);
    STL temp;
    std::swap(container, temp);
    return temp;
}

template <class STL>
STL AutoLockContainer<STL>::copy() const {
    std::shared_lock lock(mtx);
    return container;
}
