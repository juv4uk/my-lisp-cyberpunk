#include "BridgeRuntime.hpp"

#include "CanonicalReplHost.hpp"
#include "LocalReplQueue.hpp"
#include "LocalReplServer.hpp"

#include <atomic>
#include <chrono>
#include <string>
#include <thread>

namespace cyberpunk_bridge
{
namespace
{
std::atomic_bool g_stopRequested{false};
std::atomic_bool g_finished{false};
std::atomic<HANDLE> g_stoppedEvent{nullptr};

void SignalFinished()
{
    g_finished.store(true, std::memory_order_release);
    if (HANDLE event = g_stoppedEvent.load(std::memory_order_acquire); event != nullptr)
    {
        SetEvent(event);
    }
}
} // namespace

DWORD RunCanonicalRepl(HMODULE ownerModule)
{
    g_stopRequested.store(false, std::memory_order_release);
    g_finished.store(false, std::memory_order_release);

    HANDLE stoppedEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (stoppedEvent == nullptr)
    {
        g_finished.store(true, std::memory_order_release);
        return 20;
    }
    g_stoppedEvent.store(stoppedEvent, std::memory_order_release);

    CanonicalReplHost host;
    if (!host.Start(ownerModule))
    {
        SignalFinished();
        return 21;
    }

    local_repl::RequestQueue queue;
    local_repl::LocalReplServer server;
    if (!server.Start(queue))
    {
        host.Stop();
        SignalFinished();
        return 22;
    }

    while (!g_stopRequested.load(std::memory_order_acquire))
    {
        server.Drain([&host](const std::string& source) { return host.Evaluate(source); });
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }

    // Stop the socket worker before destroying the Session.  The existing
    // LocalReplServer contract cancels any still-pending request and joins the
    // worker; no socket thread ever calls my-lisp directly.
    server.Stop();
    host.Stop();
    SignalFinished();
    return 0;
}

bool ShutdownCanonicalRepl(DWORD timeoutMs)
{
    g_stopRequested.store(true, std::memory_order_release);

    const auto started = std::chrono::steady_clock::now();
    while (true)
    {
        if (g_finished.load(std::memory_order_acquire))
        {
            break;
        }

        HANDLE event = g_stoppedEvent.load(std::memory_order_acquire);
        if (event != nullptr)
        {
            const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - started);
            if (elapsed.count() >= timeoutMs)
            {
                return false;
            }
            const DWORD remaining = timeoutMs - static_cast<DWORD>(elapsed.count());
            const DWORD waitResult = WaitForSingleObject(event, remaining);
            if (waitResult != WAIT_OBJECT_0)
            {
                return false;
            }
            break;
        }

        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - started);
        if (elapsed.count() >= timeoutMs)
        {
            return false;
        }
        Sleep(1);
    }

    HANDLE event = g_stoppedEvent.exchange(nullptr, std::memory_order_acq_rel);
    if (event != nullptr)
    {
        CloseHandle(event);
    }
    return true;
}

} // namespace cyberpunk_bridge
