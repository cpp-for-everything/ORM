#pragma once

#include <atomic>
#include <memory>
#include <functional>
#include <vector>
#include <mutex>

namespace orm {

    class CancellationToken;
    class CancellationSource;

    namespace detail {

        class CancellationState
        {
            std::atomic<bool> cancelled_{false};
            std::mutex mutex_;
            std::vector<std::function<void()>> callbacks_;

        public:
            [[nodiscard]] bool is_cancelled() const noexcept
            {
                return cancelled_.load(std::memory_order_acquire);
            }

            bool cancel() noexcept
            {
                bool expected = false;
                if (cancelled_.compare_exchange_strong(
                        expected, true, std::memory_order_acq_rel))
                {
                    std::vector<std::function<void()>> cbs;
                    {
                        std::lock_guard<std::mutex> lock(mutex_);
                        cbs = std::move(callbacks_);
                    }
                    for (const auto& cb : cbs)
                    {
                        if (cb) cb();
                    }
                    return true;
                }
                return false;
            }

            bool register_callback(std::function<void()> cb)
            {
                if (is_cancelled())
                {
                    if (cb) cb();
                    return false;
                }

                std::lock_guard<std::mutex> lock(mutex_);
                if (cancelled_.load(std::memory_order_acquire))
                {
                    if (cb) cb();
                    return false;
                }
                callbacks_.push_back(std::move(cb));
                return true;
            }
        };

    } // namespace detail

    class CancellationToken
    {
        std::shared_ptr<detail::CancellationState> state_;

        friend class CancellationSource;

        explicit CancellationToken(
            std::shared_ptr<detail::CancellationState> state)
            : state_(std::move(state))
        {
        }

    public:
        CancellationToken() = default;
        CancellationToken(const CancellationToken&) = default;
        CancellationToken(CancellationToken&&) = default;
        CancellationToken& operator=(const CancellationToken&) = default;
        CancellationToken& operator=(CancellationToken&&) = default;

        [[nodiscard]] bool is_cancelled() const noexcept
        {
            return state_ && state_->is_cancelled();
        }

        explicit operator bool() const noexcept { return !is_cancelled(); }

        [[nodiscard]] bool valid() const noexcept
        {
            return state_ != nullptr;
        }

        void on_cancel(std::function<void()> callback) const
        {
            if (state_)
            {
                state_->register_callback(std::move(callback));
            }
        }

        [[nodiscard]] static CancellationToken none()
        {
            return CancellationToken{};
        }
    };

    class CancellationSource
    {
        std::shared_ptr<detail::CancellationState> state_;

    public:
        CancellationSource()
            : state_(std::make_shared<detail::CancellationState>())
        {
        }

        CancellationSource(const CancellationSource&) = default;
        CancellationSource(CancellationSource&&) = default;
        CancellationSource& operator=(const CancellationSource&) = default;
        CancellationSource& operator=(CancellationSource&&) = default;

        [[nodiscard]] CancellationToken token() const
        {
            return CancellationToken{state_};
        }

        bool cancel() noexcept
        {
            return state_ && state_->cancel();
        }

        [[nodiscard]] bool is_cancelled() const noexcept
        {
            return state_ && state_->is_cancelled();
        }
    };

    class CancellationGuard
    {
        CancellationSource* source_;

    public:
        explicit CancellationGuard(CancellationSource& source)
            : source_(&source)
        {
        }

        CancellationGuard(const CancellationGuard&) = delete;
        CancellationGuard& operator=(const CancellationGuard&) = delete;

        CancellationGuard(CancellationGuard&& other) noexcept
            : source_(other.source_)
        {
            other.source_ = nullptr;
        }

        CancellationGuard& operator=(CancellationGuard&& other) noexcept
        {
            if (this != &other)
            {
                if (source_) source_->cancel();
                source_ = other.source_;
                other.source_ = nullptr;
            }
            return *this;
        }

        ~CancellationGuard()
        {
            if (source_) source_->cancel();
        }

        void release() noexcept { source_ = nullptr; }
    };

} // namespace orm
