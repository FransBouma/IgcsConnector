#pragma once
#include <mutex>
#include <optional>
#include <queue>

namespace IGCS
{
    /// <summary>
    /// Thread safe queue. To check if the queue is empty, call pop() and check if the value is empty.
    /// From: https://codetrips.com/2020/07/26/modern-c-writing-a-thread-safe-queue/
    /// </summary>
    /// <typeparam name="T"></typeparam>
    template<typename T>
    class ThreadSafeQueue
    {
    public:
        ThreadSafeQueue() = default;
        ThreadSafeQueue(const ThreadSafeQueue<T>&) = delete;
        ThreadSafeQueue& operator=(const ThreadSafeQueue<T>&) = delete;

        ThreadSafeQueue(ThreadSafeQueue<T>&& other) noexcept
        {
            std::scoped_lock lock(mutex_);
            queue_ = std::move(other.queue_);
        }

        virtual ~ThreadSafeQueue() { }

        unsigned long size() const
        {
            std::scoped_lock lock(mutex_);
            return queue_.size();
        }

        std::optional<T> pop()
        {
            std::scoped_lock lock(mutex_);
            if(queue_.empty())
            {
                return {};
            }
            T tmp = queue_.front();
            queue_.pop();
            return tmp;
        }

        void push(const T& item)
        {
            std::scoped_lock lock(mutex_);
            queue_.push(item);
        }

    private:
        std::queue<T> queue_;
        mutable std::mutex mutex_;

        // Moved out of public interface to prevent races between this and pop().
        bool empty() const
        {
            return queue_.empty();
        }
    };
}