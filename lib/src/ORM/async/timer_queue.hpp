#pragma once
// Timers for one IoContext. The reactor loop owns the firing: it asks
// ms_until_next() for its wait timeout and calls run_due() after each wait.
// No thread is created per timer, so no thread can outlive the context.
// Timers with the same deadline fire in the order they were added.

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <functional>
#include <map>
#include <mutex>
#include <utility>
#include <vector>

namespace orm::detail {

    class TimerQueue
    {
    public:
        using Clock = std::chrono::steady_clock;

        void add(std::chrono::milliseconds delay, std::function<void()> callback)
        {
            if (delay < std::chrono::milliseconds::zero())
                delay = std::chrono::milliseconds::zero();
            std::lock_guard<std::mutex> lock(mutex_);
            timers_.emplace(Key{Clock::now() + delay, next_seq_++}, std::move(callback));
        }

        // Longest wait that does not pass the earliest deadline, at most max_ms.
        // Rounded up, so the loop does not wake early and spin.
        [[nodiscard]] int ms_until_next(int max_ms)
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (timers_.empty())
                return max_ms;
            const auto left = timers_.begin()->first.first - Clock::now();
            if (left <= Clock::duration::zero())
                return 0;
            const auto ms = std::chrono::ceil<std::chrono::milliseconds>(left).count();
            return static_cast<int>(std::min<long long>(ms, max_ms));
        }

        // Runs every timer whose deadline has passed, earliest first, outside
        // the lock, so a callback may add timers.
        void run_due()
        {
            std::vector<std::function<void()>> due;
            {
                std::lock_guard<std::mutex> lock(mutex_);
                const auto now = Clock::now();
                while (!timers_.empty() && timers_.begin()->first.first <= now)
                {
                    due.push_back(std::move(timers_.begin()->second));
                    timers_.erase(timers_.begin());
                }
            }
            for (auto& cb : due)
                if (cb) cb();
        }

        // Drops pending timers without running them.
        void cancel_all()
        {
            std::map<Key, std::function<void()>> dropped;
            {
                std::lock_guard<std::mutex> lock(mutex_);
                dropped.swap(timers_);
            }
        }

    private:
        using Key = std::pair<Clock::time_point, std::uint64_t>;

        std::mutex mutex_;
        std::map<Key, std::function<void()>> timers_;
        std::uint64_t next_seq_ = 0;
    };

} // namespace orm::detail
