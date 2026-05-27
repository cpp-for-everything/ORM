#include "ORM/async/io_context.hpp"

#if defined(_WIN32)

// clang-format off
// Order matters: winsock2.h must precede windows.h.
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
// clang-format on

#include <atomic>
#include <chrono>
#include <functional>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <thread>
#include <vector>

namespace orm {

    namespace {

        // RAII wrapper that ensures WSAStartup is called exactly once for the
        // lifetime of the process. Destructor calls WSACleanup at exit. We
        // intentionally accept the small leak on premature termination; the
        // alternative would race teardown against worker threads.
        struct WinsockInit
        {
            WinsockInit()
            {
                WSADATA data{};
                int rc = ::WSAStartup(MAKEWORD(2, 2), &data);
                if (rc != 0)
                {
                    throw std::runtime_error(
                        "WSAStartup failed with code " + std::to_string(rc));
                }
            }
            ~WinsockInit() { ::WSACleanup(); }
            WinsockInit(const WinsockInit&) = delete;
            WinsockInit& operator=(const WinsockInit&) = delete;
        };

        WinsockInit& winsock_init()
        {
            static WinsockInit instance;
            return instance;
        }

    } // namespace

    // Windows reactor.
    //
    // The file is named iocp_context.cpp for legacy reasons (the original
    // CMake referenced it) but the implementation is a readiness reactor
    // built on WSAPoll, not an IOCP completion reactor. The reason is
    // semantic: our IoContext contract is readiness-based (watch_readable /
    // watch_writable suspend until the fd is ready, then the connector calls
    // recv/send on it itself), and IOCP is fundamentally completion-based
    // (one initiates the I/O and is notified when it has completed). The two
    // models are not interchangeable without rewriting every async connector
    // to issue overlapped reads/writes.
    //
    // WSAPoll is the Windows analogue of POSIX poll(2) and exposes exactly
    // the readiness semantics we need. To wake the reactor when callbacks or
    // new registrations arrive cross-thread we keep a Windows auto-reset
    // event and AND it into every WSAPoll-with-timeout cycle via a short
    // timeout (50 ms) plus a check of the event handle.
    class IocpContext : public IoContext
    {
        struct Registration
        {
            SOCKET sock;
            PollInterest interest;
            PollOperation* op;
        };

        HANDLE wakeup_ = nullptr;
        std::vector<std::thread> workers_;
        std::atomic<bool> stopped_{false};
        size_t thread_count_;

        std::mutex callback_mutex_;
        std::queue<std::function<void()>> callbacks_;

        // Registrations are pending-list + active-list. New registrations are
        // appended to pending_ from any thread; the reactor moves them into
        // active_ at the top of each poll cycle. Active entries are removed
        // when they fire (one-shot, mirroring kqueue EV_ONESHOT).
        std::mutex registrations_mutex_;
        std::vector<Registration> pending_;
        std::vector<Registration> active_;

    public:
        explicit IocpContext(size_t thread_count)
            : thread_count_(thread_count)
        {
            winsock_init();

            wakeup_ = ::CreateEventW(nullptr, FALSE, FALSE, nullptr);
            if (!wakeup_)
            {
                throw std::runtime_error(
                    "CreateEventW failed: " + std::to_string(::GetLastError()));
            }
        }

        ~IocpContext() override
        {
            stop();

            wake();

            for (auto& worker : workers_)
            {
                if (worker.joinable())
                {
                    worker.join();
                }
            }

            if (wakeup_) ::CloseHandle(wakeup_);
        }

        void run() override
        {
            stopped_ = false;

            // WSAPoll-based reactors must be single-threaded over the same
            // fd set; we keep one reactor worker and dispatch callbacks
            // through it. Multiple workers polling the same set would race
            // on the active_ list with no improvement over a single thread.
            workers_.emplace_back([this] { worker_thread(); });

            // Additional threads (if requested) drain the callback queue but
            // do not poll; this keeps post()/schedule() responsive when the
            // reactor thread is in WSAPoll.
            for (size_t i = 1; i < thread_count_; ++i)
            {
                workers_.emplace_back([this] { callback_drain_thread(); });
            }

            for (auto& worker : workers_)
            {
                if (worker.joinable())
                {
                    worker.join();
                }
            }
            workers_.clear();
        }

        void run_one() override
        {
            promote_pending();
            poll_once(50);
            process_callbacks();
        }

        void stop() override
        {
            stopped_ = true;
            wake();
        }

        [[nodiscard]] bool stopped() const noexcept override
        {
            return stopped_;
        }

        void post(std::function<void()> callback) override
        {
            {
                std::lock_guard<std::mutex> lock(callback_mutex_);
                callbacks_.push(std::move(callback));
            }
            wake();
        }

        void schedule(
            std::chrono::milliseconds delay,
            std::function<void()> callback) override
        {
            std::thread(
                [this, delay, cb = std::move(callback)]() mutable
                {
                    std::this_thread::sleep_for(delay);
                    post(std::move(cb));
                })
                .detach();
        }

        // Reactor: register fd for readiness notification only.
        // One-shot semantics: the entry is removed from active_ after firing.
        // The "fd" passed in is interpreted as a SOCKET handle on Windows.
        void register_poll(int fd, PollOperation* op) override
        {
            Registration r;
            r.sock = static_cast<SOCKET>(static_cast<uintptr_t>(fd));
            r.interest = op->interest;
            r.op = op;

            {
                std::lock_guard<std::mutex> lock(registrations_mutex_);
                pending_.push_back(r);
            }
            wake();
        }

    private:
        void wake() noexcept
        {
            if (wakeup_) ::SetEvent(wakeup_);
        }

        void worker_thread()
        {
            while (!stopped_)
            {
                promote_pending();
                poll_once(50);
                process_callbacks();
            }
        }

        void callback_drain_thread()
        {
            while (!stopped_)
            {
                // Wait on the wakeup event with a short timeout to share the
                // load with the reactor worker without burning CPU.
                ::WaitForSingleObject(wakeup_, 50);
                process_callbacks();
            }
        }

        void promote_pending()
        {
            std::lock_guard<std::mutex> lock(registrations_mutex_);
            if (pending_.empty()) return;
            for (auto& r : pending_) active_.push_back(r);
            pending_.clear();
        }

        void poll_once(int timeout_ms)
        {
            std::vector<WSAPOLLFD> pollfds;
            std::vector<Registration> regs;

            {
                std::lock_guard<std::mutex> lock(registrations_mutex_);
                if (active_.empty())
                {
                    // No fds to wait on — just yield on the wakeup event.
                    ::WaitForSingleObject(wakeup_, timeout_ms);
                    return;
                }
                regs = active_;
            }

            pollfds.reserve(regs.size());
            for (const auto& r : regs)
            {
                WSAPOLLFD pfd{};
                pfd.fd = r.sock;
                pfd.events =
                    (r.interest == PollInterest::Read) ? POLLRDNORM : POLLWRNORM;
                pollfds.push_back(pfd);
            }

            int n = ::WSAPoll(pollfds.data(),
                              static_cast<ULONG>(pollfds.size()),
                              timeout_ms);
            if (n <= 0)
            {
                return;
            }

            // Collect ops to resume; remove the corresponding entries from
            // active_. We resume after releasing the registrations lock so a
            // coroutine that re-registers does not deadlock against us.
            std::vector<PollOperation*> to_resume;
            to_resume.reserve(static_cast<size_t>(n));

            {
                std::lock_guard<std::mutex> lock(registrations_mutex_);
                std::vector<Registration> still_active;
                still_active.reserve(active_.size());

                // Build a quick lookup from (sock, interest) → ready mask.
                // Linear scan is fine for the small N we expect.
                for (size_t i = 0; i < active_.size(); ++i)
                {
                    auto& a = active_[i];
                    bool fired = false;
                    for (size_t j = 0; j < pollfds.size(); ++j)
                    {
                        if (regs[j].sock != a.sock) continue;
                        if (regs[j].interest != a.interest) continue;
                        if (regs[j].op != a.op) continue;
                        short want =
                            (a.interest == PollInterest::Read)
                                ? (POLLRDNORM | POLLHUP | POLLERR | POLLNVAL)
                                : (POLLWRNORM | POLLHUP | POLLERR | POLLNVAL);
                        if (pollfds[j].revents & want)
                        {
                            to_resume.push_back(a.op);
                            fired = true;
                        }
                        break;
                    }
                    if (!fired)
                    {
                        still_active.push_back(a);
                    }
                }
                active_ = std::move(still_active);
            }

            for (auto* op : to_resume)
            {
                if (op && op->continuation)
                {
                    op->continuation.resume();
                }
            }
        }

        void process_callbacks()
        {
            std::function<void()> callback;
            {
                std::lock_guard<std::mutex> lock(callback_mutex_);
                if (callbacks_.empty()) return;
                callback = std::move(callbacks_.front());
                callbacks_.pop();
            }
            if (callback)
            {
                callback();
            }
        }
    };

    // Factory
    auto IoContext::create(size_t thread_count) -> std::unique_ptr<IoContext>
    {
        return std::make_unique<IocpContext>(thread_count);
    }

} // namespace orm

#endif // _WIN32
