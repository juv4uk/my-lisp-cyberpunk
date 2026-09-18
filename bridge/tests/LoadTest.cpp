// Standalone harness (NOT injected into Cyberpunk): loads our version.dll
// exactly the way any Windows process would, proves export forwarding still
// reaches the real system DLL, then requires the same bridge lifetime to
// expose one persistent canonical my-lisp Session over the existing local
// newline-delimited REPL contract.
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>

namespace
{
constexpr std::uint16_t kReplPort = 40777;
constexpr long kConnectPollMicros = 100000;
using ShutdownFn = BOOL(WINAPI*)(DWORD);
using GetSizeFn = DWORD(WINAPI*)(LPCSTR, LPDWORD);

bool ProveEarlyShutdown(const char* bridgePath)
{
    HMODULE h = LoadLibraryA(bridgePath);
    if (h == nullptr)
    {
        std::fprintf(stderr, "early-shutdown LoadLibrary failed: %lu\n", GetLastError());
        return false;
    }

    auto shutdown = reinterpret_cast<ShutdownFn>(GetProcAddress(h, "MyLispBridgeShutdown"));
    if (shutdown == nullptr)
    {
        std::fprintf(stderr, "early-shutdown export is missing\n");
        FreeLibrary(h);
        return false;
    }

    // This call intentionally races the bridge worker's startup delay.  A
    // deterministic lifecycle must preserve this stop request rather than
    // resetting it when the worker eventually creates its Session.
    const bool stopped = shutdown(5000) == TRUE;
    if (!stopped)
    {
        std::fprintf(stderr, "early MyLispBridgeShutdown timed out\n");
    }
    FreeLibrary(h);
    return stopped;
}

bool ProveSystemVersionForwarding(HMODULE bridge)
{
    auto bridgeGetSize = reinterpret_cast<GetSizeFn>(GetProcAddress(bridge, "GetFileVersionInfoSizeA"));
    if (bridgeGetSize == nullptr)
    {
        std::fprintf(stderr, "bridge GetFileVersionInfoSizeA export is missing\n");
        return false;
    }

    char systemDirectory[MAX_PATH] = {};
    const UINT length = GetSystemDirectoryA(systemDirectory, MAX_PATH);
    if (length == 0 || length >= MAX_PATH)
    {
        std::fprintf(stderr, "GetSystemDirectoryA failed: %lu\n", GetLastError());
        return false;
    }

    const std::string directory(systemDirectory, length);
    const std::string realVersionPath = directory + "\\version.dll";
    const std::string probePath = directory + "\\kernel32.dll";

    HMODULE realVersion = LoadLibraryA(realVersionPath.c_str());
    if (realVersion == nullptr)
    {
        std::fprintf(stderr, "direct system version.dll load failed: %lu\n", GetLastError());
        return false;
    }

    auto realGetSize = reinterpret_cast<GetSizeFn>(GetProcAddress(realVersion, "GetFileVersionInfoSizeA"));
    if (realGetSize == nullptr)
    {
        std::fprintf(stderr, "system version.dll GetFileVersionInfoSizeA export is missing\n");
        FreeLibrary(realVersion);
        return false;
    }

    DWORD expectedHandle = 0;
    DWORD actualHandle = 0;
    const DWORD expected = realGetSize(probePath.c_str(), &expectedHandle);
    const DWORD actual = bridgeGetSize(probePath.c_str(), &actualHandle);
    FreeLibrary(realVersion);

    if (expected == 0 || actual != expected)
    {
        std::fprintf(stderr,
                     "version forwarding mismatch for %s: system=%lu bridge=%lu\n",
                     probePath.c_str(), expected, actual);
        return false;
    }

    std::printf("version forwarding matches System32 version.dll: %lu bytes\n", actual);
    return true;
}

bool TryConnect(SOCKET client, const sockaddr_in& address)
{
    u_long nonBlocking = 1;
    if (ioctlsocket(client, FIONBIO, &nonBlocking) != 0)
    {
        return false;
    }

    const int result = connect(client, reinterpret_cast<const sockaddr*>(&address), sizeof(address));
    if (result != 0)
    {
        const int error = WSAGetLastError();
        if (error != WSAEWOULDBLOCK && error != WSAEINPROGRESS && error != WSAEINVAL)
        {
            return false;
        }

        fd_set writable;
        fd_set failed;
        FD_ZERO(&writable);
        FD_ZERO(&failed);
        FD_SET(client, &writable);
        FD_SET(client, &failed);
        timeval timeout{};
        timeout.tv_sec = 0;
        timeout.tv_usec = kConnectPollMicros;
        const int ready = select(0, nullptr, &writable, &failed, &timeout);
        if (ready <= 0 || FD_ISSET(client, &failed))
        {
            return false;
        }

        int socketError = 0;
        int socketErrorSize = sizeof(socketError);
        if (getsockopt(client, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&socketError), &socketErrorSize) != 0 ||
            socketError != 0)
        {
            return false;
        }
    }

    nonBlocking = 0;
    if (ioctlsocket(client, FIONBIO, &nonBlocking) != 0)
    {
        return false;
    }

    const DWORD ioTimeoutMs = 2000;
    setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&ioTimeoutMs), sizeof(ioTimeoutMs));
    setsockopt(client, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char*>(&ioTimeoutMs), sizeof(ioTimeoutMs));
    return true;
}

SOCKET ConnectRepl()
{
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(kReplPort);
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    for (int attempt = 0; attempt < 50; ++attempt)
    {
        SOCKET client = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (client == INVALID_SOCKET)
        {
            return INVALID_SOCKET;
        }
        if (TryConnect(client, address))
        {
            return client;
        }
        closesocket(client);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    return INVALID_SOCKET;
}

bool Exchange(SOCKET client, const std::string& source, std::string& reply)
{
    const std::string request = source + "\n";
    if (send(client, request.data(), static_cast<int>(request.size()), 0) != static_cast<int>(request.size()))
    {
        return false;
    }

    reply.clear();
    char chunk[512];
    while (reply.find('\n') == std::string::npos)
    {
        const int received = recv(client, chunk, sizeof(chunk), 0);
        if (received <= 0)
        {
            return false;
        }
        reply.append(chunk, static_cast<std::size_t>(received));
        if (reply.size() > 64 * 1024)
        {
            return false;
        }
    }

    reply.resize(reply.find('\n'));
    if (!reply.empty() && reply.back() == '\r')
    {
        reply.pop_back();
    }
    return true;
}

bool RequireReply(SOCKET client, const char* source, const char* expected)
{
    std::string reply;
    if (!Exchange(client, source, reply))
    {
        std::fprintf(stderr, "REPL request failed: %s\n", source);
        return false;
    }
    if (reply != expected)
    {
        std::fprintf(stderr, "REPL mismatch for %s: expected '%s', got '%s'\n",
                     source, expected, reply.c_str());
        return false;
    }
    return true;
}

std::string ReadText(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    if (!input)
    {
        return {};
    }
    std::ostringstream text;
    text << input.rdbuf();
    return text.str();
}

bool WriteText(const std::filesystem::path& path, const std::string& text)
{
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output)
    {
        return false;
    }
    output.write(text.data(), static_cast<std::streamsize>(text.size()));
    return output.good();
}

bool PreparePluginFixtures(const std::filesystem::path& bridgePath)
{
    const auto root = bridgePath.parent_path() / "my-lisp";
    std::error_code error;
    std::filesystem::remove_all(root, error);
    error.clear();
    std::filesystem::create_directories(root / "plugins", error);
    if (error)
    {
        std::fprintf(stderr, "could not create plugin fixture directory: %s\n", error.message().c_str());
        return false;
    }

    struct Fixture
    {
        const char* relativePath;
        const char* source;
    };
    const Fixture fixtures[] = {
        {"init.lisp", u8"(\u0432\u0438\u0437\u043d\u0430\u0447\u0438\u0442\u0438 plugin-base 10)\n"},
        {"plugins/a-first.lisp", u8"(\u0432\u0438\u0437\u043d\u0430\u0447\u0438\u0442\u0438 plugin-first (+ plugin-base 1))\n"},
        {"plugins/b-broken.lisp", "(this-plugin-is-broken 1)\n"},
        {"plugins/c-after.lisp", u8"(\u0432\u0438\u0437\u043d\u0430\u0447\u0438\u0442\u0438 plugin-after (+ plugin-first 31))\n"},
        {"plugins/notes.md", "not a Lisp plugin\n"},
    };

    for (const auto& fixture : fixtures)
    {
        const auto path = root / fixture.relativePath;
        if (!WriteText(path, fixture.source))
        {
            std::fprintf(stderr, "could not write plugin fixture: %s\n", path.string().c_str());
            return false;
        }
    }
    return true;
}

bool RequireDefinedWithoutOwningValue(SOCKET client, const char* symbol)
{
    std::string reply;
    if (!Exchange(client, symbol, reply))
    {
        std::fprintf(stderr, "plugin symbol request failed: %s\n", symbol);
        return false;
    }
    if (reply.rfind("error:", 0) == 0)
    {
        std::fprintf(stderr, "plugin symbol was not available in canonical Session: %s -> %s\n",
                     symbol, reply.c_str());
        return false;
    }
    return true;
}

bool ProvePluginLoadReport(const std::filesystem::path& bridgePath)
{
    const auto observationPath = bridgePath.parent_path() / "bridge-observation.lisp";
    const std::string observation = ReadText(observationPath);
    if (observation.find("(plugin-load-report/1") == std::string::npos)
    {
        std::fprintf(stderr, "plugin load report missing from runtime observation\n");
        return false;
    }

    struct Expected
    {
        const char* path;
        const char* sha256;
        const char* status;
    };
    const Expected expected[] = {
        {"init.lisp", "95dbdb59aad65dfabda6c52e8c1a78dca88ccee38bbcbe2893ab46ead20313d7", "loaded"},
        {"plugins/a-first.lisp", "4be2385e389773d68bda1f55f0d0acc20db1933f297237a13c6f3ed3d615da50", "loaded"},
        {"plugins/b-broken.lisp", "da9356697bf1721ecf1378bc0c40a2929b01bb6fdb827bd6bb06afa86f398dce", "error"},
        {"plugins/c-after.lisp", "6095af27bb858247b056d561768d7b4054551b9d3e3bb0e29ec689a7cc27e657", "loaded"},
    };

    std::size_t previous = 0;
    bool first = true;
    for (const auto& item : expected)
    {
        const std::string needle =
            "((path \"" + std::string(item.path) + "\") (sha256 \"" + item.sha256 +
            "\") (status " + item.status + "))";
        const auto position = observation.find(needle);
        if (position == std::string::npos)
        {
            std::fprintf(stderr, "plugin report entry missing: %s\n", needle.c_str());
            return false;
        }
        if (!first && position <= previous)
        {
            std::fprintf(stderr, "plugin report order is not deterministic around: %s\n", item.path);
            return false;
        }
        previous = position;
        first = false;
    }

    if (observation.find("plugins/notes.md") != std::string::npos)
    {
        std::fprintf(stderr, "non-.lisp file was admitted into plugin report\n");
        return false;
    }
    return true;
}

bool ProveRuntimeProvenance(const std::filesystem::path& bridgePath,
                            const std::string& expectedSha,
                            const std::string& expectedAbi)
{
    if (expectedSha.size() != 40 || expectedAbi.empty())
    {
        std::fprintf(stderr, "test evidence lacks exact expected SHA/ABI\n");
        return false;
    }

    const std::filesystem::path observationPath = bridgePath.parent_path() / "bridge-observation.lisp";
    const std::string observation = ReadText(observationPath);
    if (observation.empty())
    {
        std::fprintf(stderr, "bridge observation missing: %s\n", observationPath.string().c_str());
        return false;
    }

    const std::string shaFact = "(my-lisp-sha \"" + expectedSha + "\")";
    const std::string abiFact = "(embed-abi " + expectedAbi + ")";
    const std::string linkageFact = "(linkage static)";
    if (observation.find(shaFact) == std::string::npos)
    {
        std::fprintf(stderr, "bridge observation missing exact my-lisp SHA: %s\n", expectedSha.c_str());
        return false;
    }
    if (observation.find(abiFact) == std::string::npos)
    {
        std::fprintf(stderr, "bridge observation missing accepted embed ABI: %s\n", expectedAbi.c_str());
        return false;
    }
    if (observation.find(linkageFact) == std::string::npos)
    {
        std::fprintf(stderr, "bridge observation missing static linkage provenance\n");
        return false;
    }
    return true;
}
} // namespace

int main(int argc, char** argv)
{
    if (argc < 4)
    {
        std::fprintf(stderr, "usage: LoadTest <path to version.dll> <expected my-lisp SHA> <expected embed ABI>\n");
        return 1;
    }

    if (!ProveEarlyShutdown(argv[1]))
    {
        return 2;
    }
    std::printf("early shutdown witness OK\n");

    const std::filesystem::path bridgePath = std::filesystem::absolute(argv[1]);
    if (!PreparePluginFixtures(bridgePath))
    {
        return 15;
    }

    WSADATA winsock{};
    if (WSAStartup(MAKEWORD(2, 2), &winsock) != 0)
    {
        std::fprintf(stderr, "WSAStartup failed\n");
        return 3;
    }

    HMODULE h = LoadLibraryA(argv[1]);
    if (h == nullptr)
    {
        std::fprintf(stderr, "LoadLibrary failed: %lu\n", GetLastError());
        WSACleanup();
        return 4;
    }
    std::printf("LoadLibrary OK, module=%p\n", static_cast<void*>(h));

    auto shutdown = reinterpret_cast<ShutdownFn>(GetProcAddress(h, "MyLispBridgeShutdown"));
    if (shutdown == nullptr)
    {
        std::fprintf(stderr, "required bridge lifecycle export is missing\n");
        FreeLibrary(h);
        WSACleanup();
        return 5;
    }

    if (!ProveSystemVersionForwarding(h))
    {
        shutdown(5000);
        FreeLibrary(h);
        WSACleanup();
        return 6;
    }

    auto StopAndUnload = [&](SOCKET client) -> bool
    {
        if (client != INVALID_SOCKET)
        {
            closesocket(client);
        }
        const bool stopped = shutdown(5000) == TRUE;
        if (!stopped)
        {
            std::fprintf(stderr, "MyLispBridgeShutdown timed out\n");
        }
        FreeLibrary(h);
        WSACleanup();
        return stopped;
    };

    SOCKET client = ConnectRepl();
    if (client == INVALID_SOCKET)
    {
        std::fprintf(stderr, "canonical REPL did not appear on 127.0.0.1:%u\n", kReplPort);
        StopAndUnload(INVALID_SOCKET);
        return 7;
    }

    // Plugin loading is a mechanism witness, not a second semantic oracle:
    // only prove that definitions from init + later plugins exist in the same
    // canonical Session. Their Lisp values remain owned by canonical my-lisp.
    if (!RequireDefinedWithoutOwningValue(client, "plugin-base") ||
        !RequireDefinedWithoutOwningValue(client, "plugin-first") ||
        !RequireDefinedWithoutOwningValue(client, "plugin-after"))
    {
        StopAndUnload(client);
        return 16;
    }

    // Replay the exact Ukrainian forms already proved by the pinned
    // my-lisp-embed tests. u8 + universal character names makes this input
    // deterministic UTF-8 regardless of the Windows runner's local code page.
    constexpr const char* defineValue =
        u8"(\u0432\u0438\u0437\u043d\u0430\u0447\u0438\u0442\u0438 repl-\u043f\u0435\u0440\u0435\u0432\u0456\u0440\u043a\u0430 42)";
    constexpr const char* readValue =
        u8"repl-\u043f\u0435\u0440\u0435\u0432\u0456\u0440\u043a\u0430";
    constexpr const char* defineClosure =
        u8"(\u0432\u0438\u0437\u043d\u0430\u0447\u0438\u0442\u0438 \u043f\u043e\u0434\u0432\u043e\u0457\u0442\u0438 (\u0444\u0443\u043d\u043a\u0446\u0456\u044f (\u0437\u043d\u0430\u0447\u0435\u043d\u043d\u044f) (+ \u0437\u043d\u0430\u0447\u0435\u043d\u043d\u044f \u0437\u043d\u0430\u0447\u0435\u043d\u043d\u044f)))";
    constexpr const char* callClosure =
        u8"(\u043f\u043e\u0434\u0432\u043e\u0457\u0442\u0438 21)";

    if (!RequireReply(client, defineValue, "42"))
    {
        StopAndUnload(client);
        return 8;
    }

    // The transport is not Session authority: disconnecting a client must not
    // destroy Lisp state. Reconnect before both variable readback and closure
    // invocation so one canonical Session is proved across three clients.
    closesocket(client);
    client = ConnectRepl();
    if (client == INVALID_SOCKET || !RequireReply(client, readValue, "42") ||
        !RequireReply(client, defineClosure, "<lambda>"))
    {
        StopAndUnload(client);
        return 9;
    }

    closesocket(client);
    client = ConnectRepl();
    if (client == INVALID_SOCKET || !RequireReply(client, callClosure, "42"))
    {
        StopAndUnload(client);
        return 10;
    }

    std::string error;
    if (!Exchange(client, "(car 7)", error) || error.rfind("error:", 0) != 0)
    {
        std::fprintf(stderr, "expected canonical error, got '%s'\n", error.c_str());
        StopAndUnload(client);
        return 11;
    }
    if (!RequireReply(client, "(+ 20 22)", "42"))
    {
        StopAndUnload(client);
        return 12;
    }

    if (!StopAndUnload(client))
    {
        return 13;
    }
    if (!ProveRuntimeProvenance(bridgePath, argv[2], argv[3]))
    {
        return 14;
    }
    if (!ProvePluginLoadReport(bridgePath))
    {
        return 17;
    }
    std::printf("canonical Ukrainian REPL persisted across reconnects + error + shutdown + compiled provenance witness OK\n");
    return 0;
}
