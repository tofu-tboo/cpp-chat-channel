#ifndef __AUTO_LOCK_CONTAINER_H__
#define __AUTO_LOCK_CONTAINER_H__

#include <shared_mutex>
#include <concepts>

// Concepts for STL traits
namespace ALC_Traits {
    template<typename T> concept HasPushBack = requires(T t, typename T::value_type v) { t.push_back(v); };
    template<typename T> concept HasPush = requires(T t, typename T::value_type v) { t.push(v); };
    template<typename T> concept HasKey = requires(T t) { typename T::key_type; };
    template<typename T> concept HasMapped = requires(T t) { typename T::mapped_type; };
    template<typename T> concept HasFront = requires(T t) { t.front(); };
    template<typename T> concept HasTop = requires(T t) { t.top(); };
    
    // Nested Container Traits
    template<typename T> concept MappedHasPush = requires(typename T::mapped_type t, typename T::mapped_type::value_type v) { t.push(v); };
    template<typename T> concept MappedHasPushBack = requires(typename T::mapped_type t, typename T::mapped_type::value_type v) { t.push_back(v); };
    template<typename T> concept MappedHasInsert = requires(typename T::mapped_type t, typename T::mapped_type::value_type v) { t.insert(v); };
    template<typename T> concept HasClear = requires(T t) { t.clear(); };
}

template <class STL>
class AutoLockContainer {
private:
    STL container;
    mutable std::shared_mutex mtx; // for const func

public:
    using value_type = typename STL::value_type;

    // 1. Add (Unified)
    // Case A: Simple Container (Vector, Queue, Set)
    void add(const value_type& item) requires (ALC_Traits::HasPushBack<STL> || ALC_Traits::HasPush<STL> || (ALC_Traits::HasKey<STL> && !ALC_Traits::HasMapped<STL>));
    
    // Case B: Map (Key-Value Update/Insert)
    template <typename Key, typename Value>
    void add(const Key& key, const Value& value) requires ALC_Traits::HasMapped<STL> && std::convertible_to<Value, typename STL::mapped_type>;

    // Case C: Nested Container Push (Map<Key, Container> -> container.push(SubValue))
    template <typename Key, typename SubValue>
    void add(const Key& key, const SubValue& subval) 
        requires ALC_Traits::HasMapped<STL> 
              && (ALC_Traits::MappedHasPush<STL> || ALC_Traits::MappedHasPushBack<STL> || ALC_Traits::MappedHasInsert<STL>)
              && (!std::convertible_to<SubValue, typename STL::mapped_type>);

    // 2. Delete (Unified)
    // Case A: Pop (Queue, Stack)
    bool del(value_type& out_item) requires (ALC_Traits::HasFront<STL> || ALC_Traits::HasTop<STL>);
    // Case B: Erase (Map, Set)
    template <typename Key>
    bool del(const Key& key) requires ALC_Traits::HasKey<STL>;

    // 3. Read (Unified)
    bool get(value_type& out_item) const requires (ALC_Traits::HasFront<STL> || ALC_Traits::HasTop<STL>);
    template <typename Key>
    bool get(const Key& key, typename STL::mapped_type& out_value) const requires ALC_Traits::HasMapped<STL>;

    // 5. Utilities
    size_t size() const;
    bool empty() const;
    
    void clear() requires ALC_Traits::HasClear<STL>;
    void clear() requires (!ALC_Traits::HasClear<STL>); // For adapters like queue

    template <typename Key>
    bool exist(const Key& key) const requires ALC_Traits::HasKey<STL>;

    // 6. Task (Manual Locking)
    template <typename Func>
    void task(Func&& func);

    // Move internal container out (Reset)
    STL move();
};

#include "auto_lock_container.tpp"

#endif