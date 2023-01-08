#pragma once
#include "agano.hpp"
#include <algorithm>
#include <concepts>
#include <exception>
#include <thread>
#include <type_traits>
#include <mutex>

#include <fuwa/types.hpp>
#include <fuwa/assert.hpp>

namespace agano {
    template<typename T>
    inline constexpr bool send_tag_v = false;

    template<typename T>
    concept Send = send_tag_v<std::remove_cvref_t<T>> && std::movable<std::remove_cvref_t<T>>;

    template<typename T>
    concept Sync = send_tag_v<const std::remove_cvref_t<T>&>;

    template<typename T>
    concept Mutex = requires(T obj) {
        obj.lock();
        obj.unlock();
        typename std::lock_guard<T>;
    };

    template<Send T, Mutex M = std::mutex>
    class Locked {
    private:
        T& ref_;
        std::lock_guard<M> lock_;

    public:
        Locked(T& ref, M& mutex) noexcept 
            : ref_{ ref }
            , lock_{ mutex }
        {}

        Locked(Locked&&) noexcept = delete;
        Locked& operator=(Locked&&) noexcept = delete;

        Locked(const Locked&) = delete;
        Locked& operator=(const Locked&) = delete;

        ~Locked() noexcept = default;

        const T*  operator->() const noexcept {
            return &ref_;
        }

        const T& operator*() const noexcept {
            return ref_;
        }

        T* operator->() noexcept {
            return &ref_;
        }

        T& operator*() noexcept {
            return ref_;
        }
    };

    template<Send T, Mutex M = std::mutex>
    class Synced {
    private:
        M mutex_;
        T owned_;

    public:
        Synced() noexcept = default;
        Synced(T&& rhs) noexcept 
            : owned_{ std::move(rhs) }
        {}

        Synced(Synced&&) noexcept = delete;
        Synced& operator=(Synced&&) noexcept = delete;

        Synced(const Synced&) = delete;
        Synced& operator=(const Synced&) = delete;

        ~Synced() noexcept = default;

        [[nodiscard]]
        Locked<T, M> lock() noexcept {
            return Locked{ owned_, mutex_ };
        }

    };

    struct DeferBindingTag{};
    inline constexpr DeferBindingTag defer_binding{};

    /*
    * ThreadBound<T> is a wrapper that owns an object of type T and is bound to thread the wrapper was created.
    * Once an instance of ThreadBound<T> gets bound to a thread, only this thread can access 
    * an owned object. You can copy this instance and the owned object will be copied too. 
    * For moving an ownership from one thread to other use std::move. Old instance will get unbound and
    * the object will get transferred to moved-from state. 
    * Any access to object from a thread that does not own this object will cause a run-time error.
    */
    template<std::movable T>
    class ThreadBound {
    private:
        static const std::thread::id unbound;

        T object;
        mutable std::thread::id owner_id;

    public:
        ThreadBound(T&& obj) noexcept
            : object{ std::move(obj) }
            , owner_id{ std::this_thread::get_id() }
        {}

        ThreadBound(DeferBindingTag, T&& obj) noexcept 
            : object{ std::move(obj) }
        {}

        ThreadBound(const ThreadBound& rhs) noexcept 
            : object{ rhs.object }
            , owner_id{ get_owner_id_(rhs) }
        {}

        ThreadBound& operator=(const ThreadBound& rhs) noexcept {
            if (&rhs == this) {
                return *this;
            }

            object = rhs.object;
            set_owner_id_(rhs);

            return *this;
        }

        ThreadBound(ThreadBound&& rhs) noexcept 
            : object{ std::move(rhs.object) }
            , owner_id{ get_owner_id_(rhs) }
        {
            rhs.owner_id = unbound;
        }

        ThreadBound& operator=(ThreadBound&& rhs) noexcept {
            if (&rhs == this) {
                return *this;
            }

            object = std::move(rhs.object);
            set_owner_id_(rhs);

            rhs.owner_id = unbound;

            return *this;
        }

        ~ThreadBound() noexcept = default;

        T& operator*() noexcept {
            validate_();
            return object;
        }

        T* operator->() noexcept {
            validate_();
            return &object;
        }

        const T& operator*() const noexcept {
            validate_();
            return object;
        }

        const T* operator->() const noexcept {
            validate_();
            return &object;
        }

        bool is_unbound() const noexcept {
            return owner_id == unbound;
        }

        std::thread::id get_owner_id() const noexcept {
            return owner_id;
        }

    private:
        void validate_() const noexcept {
            if (is_unbound()) {
                owner_id = std::this_thread::get_id();
                return;
            }

            if (std::this_thread::get_id() != owner_id) {
                static std::hash<std::thread::id> hasher;
                EH_PANIC(std::format("Thread-safety violation occurred! The object is bound to one thread (id: {}), but used in other (id: {})", hasher(owner_id), hasher(std::this_thread::get_id())));
            }
        }

        std::thread::id get_owner_id_(const ThreadBound& rhs) {
            if (rhs.is_unbound()) {
                return unbound;
            }

            return std::this_thread::get_id();
        }
    };

    template<std::movable T>
    inline const std::thread::id ThreadBound<T>::unbound{}; 

    template<std::movable T, typename... Args>
    ThreadBound<T> make_thread_bound(Args&&... args) noexcept {
        return ThreadBound<T>{ T{ std::forward<Args>(args)... } };
    }

    template<std::movable T, typename... Args>
    ThreadBound<T> make_thread_bound(DeferBindingTag, Args&&... args) noexcept {
        return ThreadBound<T>{ defer_binding, T{ std::forward<Args>(args)... } };
    }

    template<Send T, Mutex M>
    inline constexpr bool send_tag_v<Synced<T, M>> = true;

    template<Send T, Mutex M>
    inline constexpr bool send_tag_v<Synced<T, M>&> = true;

    template<Send T, Mutex M>
    inline constexpr bool send_tag_v<const Synced<T, M>&> = true;

    template<>
    inline constexpr bool send_tag_v<int> = true;
}