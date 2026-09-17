#include "BridgeRuntime.hpp"

#include "CanonicalReplHost.hpp"
#include "LocalReplQueue.hpp"
#include "LocalReplServer.hpp"

#include <atomic>
#include <chrono>
#include <cctype>
#include <cstdio>
#include <cstdlib>
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

std::wstring AdjacentPath(HMODULE ownerModule, const wchar_t* fileName)
{
    wchar_t modulePath[MAX_PATH] = {};
    const DWORD length = GetModuleFileNameW(ownerModule, modulePath, MAX_PATH);
    if (length == 0 || length == MAX_PATH)
    {
        return {};
    }

    std::wstring path(modulePath, length);
    const auto slash = path.find_last_of(L"\\/");
    if (slash == std::wstring::npos)
    {
        return {};
    }
    path.resize(slash + 1);
    path += fileName;
    return path;
}

bool IsExactSha(const std::string& sha)
{
    if (sha.size() != 40)
    {
        return false;
    }
    for (const unsigned char ch : sha)
    {
        if (!std::isxdigit(ch))
        {
            return false;
        }
    }
    return true;
}

bool ReadCanonicalProvenance(HMODULE ownerModule, std::string& sha, std::uint32_t& declaredAbi)
{
    const std::wstring provenancePath = AdjacentPath(ownerModule, L"cyberpunk-my-lisp-provenance.txt");
    if (provenancePath.empty())
    {
        return false;
    }

    FILE* file = nullptr;
    if (_wfopen_s(&file, provenancePath.c_str(), L"rb") != 0 || file == nullptr)
    {
        return false;
    }

    std::string text;
    char chunk[512];
    while (const std::size_t read = std::fread(chunk, 1, sizeof(chunk), file))
    {
        text.append(chunk, read);
        if (text.size() > 16 * 1024)
        {
            std::fclose(file);
            return false;
        }
    }
    std::fclose(file);

    auto valueFor = [&text](const char* key) -> std::string
    {
        const std::string prefix = std::string(key) + "=";
        const std::size_t found = text.find(prefix);
        if (found == std::string::npos)
        {
            return {};
        }
        const std::size_t start = found + prefix.size();
        const std::size_t end = text.find_first_of("\r\n", start);
        return text.substr(start, end == std::string::npos ? std::string::npos : end - start);
    };

    sha = valueFor("my-lisp-sha");
    const std::string abiText = valueFor("embed-abi");
    if (!IsExactSha(sha) || abiText.empty())
    {
        return false;
    }

    char* end = nullptr;
    const unsigned long parsed = std::strtoul(abiText.c_str(), &end, 10);
    if (end == abiText.c_str() || *end != '\0' || parsed > 0xFFFFFFFFUL)
    {
        return false;
    }
    declaredAbi = static_cast<std::uint32_t>(parsed);
    return true;
}

bool RecordAcceptedCanonicalProvenance(HMODULE ownerModule, std::uint32_t acceptedAbi)
{
    std::string sha;
    std::uint32_t declaredAbi = 0;
    if (!ReadCanonicalProvenance(ownerModule, sha, declaredAbi) || declaredAbi != acceptedAbi)
    {
        return false;
    }

    const std::wstring observationPath = AdjacentPath(ownerModule, L"bridge-observation.lisp");
    if (observationPath.empty())
    {
        return false;
    }

    FILE* file = nullptr;
    if (_wfopen_s(&file, observationPath.c_str(), L"a") != 0 || file == nullptr)
    {
        return false;
    }

    // Provenance only: no Lisp or gameplay semantics are decided here.  The
    // SHA comes from the build-produced canonical provenance file; the ABI is
    // the version actually accepted from the loaded my-lisp-embed DLL.
    const int written = std::fprintf(
        file,
        "(bridge-runtime-provenance/1 (my-lisp-sha \"%s\") (embed-abi %u))\n",
        sha.c_str(), static_cast<unsigned int>(acceptedAbi));
    std::fclose(file);
    return written > 0;
}
} // namespace

void PrepareCanonicalRepl()
{
    // DllMain calls this before the worker exists.  Once the worker has been
    // created, only ShutdownCanonicalRepl may move stopRequested to true; the
    // worker must never erase that request during delayed startup.
    g_stopRequested.store(false, std::memory_order_release);
    g_finished.store(false, std::memory_order_release);
    g_stoppedEvent.store(nullptr, std::memory_order_release);
}

DWORD RunCanonicalRepl(HMODULE ownerModule)
{
    HANDLE stoppedEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (stoppedEvent == nullptr)
    {
        g_finished.store(true, std::memory_order_release);
        return 20;
    }
    g_stoppedEvent.store(stoppedEvent, std::memory_order_release);

    // Shutdown may have been requested immediately after LoadLibrary, while
    // this worker was still delayed outside the loader lock.  Preserve that
    // request and finish without ever creating a canonical Session.
    if (g_stopRequested.load(std::memory_order_acquire))
    {
        SignalFinished();
        return 0;
    }

    CanonicalReplHost host;
    if (!host.Start(ownerModule))
    {
        SignalFinished();
        return 21;
    }

    if (!RecordAcceptedCanonicalProvenance(ownerModule, host.AbiVersion()))
    {
        host.Stop();
        SignalFinished();
        return 23;
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
