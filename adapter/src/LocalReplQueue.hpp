#pragma once

#include <cstddef>
#include <functional>
#include <mutex>
#include <optional>
#include <queue>
#include <string>
#include <utility>

namespace local_repl
{
constexpr std::size_t kMaxRequestBytes = 16 * 1024;
constexpr std::size_t kMaxPendingRequests = 32;

using Reply = std::function<void(std::string)>;

struct Request
{
    std::string source;
    Reply reply;
};

// Transport-only boundary between a local socket worker and RED4ext's game
// thread. It intentionally contains no evaluator or engine access.
class RequestQueue
{
public:
    [[nodiscard]] bool Push(std::string source, Reply reply)
    {
        if (source.size() > kMaxRequestBytes || !reply)
        {
            return false;
        }

        std::lock_guard lock(m_mutex);
        if (m_requests.size() == kMaxPendingRequests)
        {
            return false;
        }
        m_requests.push(Request{std::move(source), std::move(reply)});
        return true;
    }

    [[nodiscard]] std::optional<Request> Pop()
    {
        std::lock_guard lock(m_mutex);
        if (m_requests.empty())
        {
            return std::nullopt;
        }
        Request request = std::move(m_requests.front());
        m_requests.pop();
        return request;
    }

    [[nodiscard]] std::size_t Pending() const
    {
        std::lock_guard lock(m_mutex);
        return m_requests.size();
    }

    [[nodiscard]] std::size_t CancelAll(std::string result)
    {
        std::queue<Request> cancelled;
        {
            std::lock_guard lock(m_mutex);
            cancelled.swap(m_requests);
        }

        const std::size_t count = cancelled.size();
        while (!cancelled.empty())
        {
            Request request = std::move(cancelled.front());
            cancelled.pop();
            request.reply(result);
        }
        return count;
    }

private:
    mutable std::mutex m_mutex;
    std::queue<Request> m_requests;
};
} // namespace local_repl
