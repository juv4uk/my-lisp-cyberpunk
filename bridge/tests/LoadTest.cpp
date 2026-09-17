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

std::string ProvenanceValue(const std::string& text, const std::string& key)
{
    const std::string prefix = key + "=";
    std::size_t start = text.find(prefix);
    if (start == std::string::npos)
    {
        return {};
    }
    start += prefix.size();
    std::size_t end = text.find_first_of("\r\n", start);
    return text.substr(start, end == std::string::npos ? std::string::npos : end - start);
}

bool ProveRuntimeProvenance(const std::filesystem::path& bridgePath)
{
    const std::filesystem::path directory = bridgePath.parent_path();
    const std::filesystem::path provenancePath = directory / "cyberpunk-my-lisp-provenance.txt";
    const std::filesystem::path observationPath = directory / "bridge-observation.lisp";

    const std::string provenance = ReadText(provenancePath);
    if (provenance.empty())
    {
        std::fprintf(stderr, "canonical provenance file missing beside bridge: %s\n",
                     provenancePath.string().c_str());
        return false;
    }

    const std::string sha = ProvenanceValue(provenance, "my-lisp-sha");
    const std::string abi = ProvenanceValue(provenance, "embed-abi");
    if (sha.size() != 40 || abi.empty())
    {
        std::fprintf(stderr, "canonical provenance file lacks exact SHA/ABI\n");
        return false;
    }

    const std::string observation = ReadText(observationPath);
    if (observation.empty())
    {
        std::fprintf(stderr, "bridge observation missing: %s\n", observationPath.string().c_str());
        return false;
    }

    const std::string shaFact = "(my-lisp-sha \"" + sha + "\")";
    const std::string abiFact = "(embed-abi " + abi + ")";
    if (observation.find(shaFact) == std::string::npos)
    {
        std::fprintf(stderr, "bridge observation missing exact my-lisp SHA: %s\n", sha.c_str());
        return false;
    }
    if (observation.find(abiFact) == std::string::npos)
    {
        std::fprintf(stderr, "bridge observation missing accepted embed ABI: %s\n", abi.c_str());
        return false;
    }
    return true;
}
} // namespace

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        std::fprintf(stderr, "usage: LoadTest <path to version.dll>\n");
        return 1;
    }

    if (!ProveEarlyShutdown(argv[1]))
    {
        return 2;
    }
    std::printf("early shutdown witness OK\n");

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

    using GetSizeFn = DWORD(WINAPI*)(LPCSTR, LPDWORD);
    auto getSize = reinterpret_cast<GetSizeFn>(GetProcAddress(h, "GetFileVersionInfoSizeA"));
    auto shutdown = reinterpret_cast<ShutdownFn>(GetProcAddress(h, "MyLispBridgeShutdown"));
    if (getSize == nullptr || shutdown == nullptr)
    {
        std::fprintf(stderr, "required bridge exports are missing\n");
        FreeLibrary(h);
        WSACleanup();
        return 5;
    }

    DWORD handle = 0;
    const DWORD size = getSize(argv[1], &handle);
    std::printf("forwarded GetFileVersionInfoSizeA(%s) = %lu\n", argv[1], size);

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
        return 6;
    }

    std::string ignored;
    if (!Exchange(client, "(define x 42)", ignored) ||
        !RequireReply(client, "x", "42") ||
        !Exchange(client, "(define twice (lambda (value) (+ value value)))", ignored) ||
        !RequireReply(client, "(twice 21)", "42"))
    {
        StopAndUnload(client);
        return 7;
    }

    std::string error;
    if (!Exchange(client, "(car 5)", error) || error.rfind("error:", 0) != 0)
    {
        std::fprintf(stderr, "expected canonical error, got '%s'\n", error.c_str());
        StopAndUnload(client);
        return 8;
    }
    if (!RequireReply(client, "(+ 20 22)", "42"))
    {
        StopAndUnload(client);
        return 9;
    }

    if (!StopAndUnload(client))
    {
        return 10;
    }
    if (!ProveRuntimeProvenance(std::filesystem::path(argv[1])))
    {
        return 11;
    }
    std::printf("persistent canonical REPL + shutdown + provenance witness OK\n");
    return 0;
}
