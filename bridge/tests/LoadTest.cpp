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
#include <string>
#include <thread>

namespace
{
constexpr std::uint16_t kReplPort = 40777;

SOCKET ConnectRepl()
{
    for (int attempt = 0; attempt < 80; ++attempt)
    {
        SOCKET client = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (client == INVALID_SOCKET)
        {
            return INVALID_SOCKET;
        }

        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_port = htons(kReplPort);
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        if (connect(client, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) == 0)
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
} // namespace

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        std::fprintf(stderr, "usage: LoadTest <path to version.dll>\n");
        return 1;
    }

    WSADATA winsock{};
    if (WSAStartup(MAKEWORD(2, 2), &winsock) != 0)
    {
        std::fprintf(stderr, "WSAStartup failed\n");
        return 2;
    }

    HMODULE h = LoadLibraryA(argv[1]);
    if (h == nullptr)
    {
        std::fprintf(stderr, "LoadLibrary failed: %lu\n", GetLastError());
        WSACleanup();
        return 3;
    }
    std::printf("LoadLibrary OK, module=%p\n", static_cast<void*>(h));

    using GetSizeFn = DWORD(WINAPI*)(LPCSTR, LPDWORD);
    auto getSize = reinterpret_cast<GetSizeFn>(GetProcAddress(h, "GetFileVersionInfoSizeA"));
    if (getSize == nullptr)
    {
        std::fprintf(stderr, "GetProcAddress(GetFileVersionInfoSizeA) failed\n");
        FreeLibrary(h);
        WSACleanup();
        return 4;
    }

    DWORD handle = 0;
    const DWORD size = getSize(argv[1], &handle);
    std::printf("forwarded GetFileVersionInfoSizeA(%s) = %lu\n", argv[1], size);

    SOCKET client = ConnectRepl();
    if (client == INVALID_SOCKET)
    {
        std::fprintf(stderr, "canonical REPL did not appear on 127.0.0.1:%u\n", kReplPort);
        FreeLibrary(h);
        WSACleanup();
        return 5;
    }

    std::string ignored;
    if (!Exchange(client, "(define x 42)", ignored) ||
        !RequireReply(client, "x", "42") ||
        !Exchange(client, "(define twice (lambda (value) (+ value value)))", ignored) ||
        !RequireReply(client, "(twice 21)", "42"))
    {
        closesocket(client);
        FreeLibrary(h);
        WSACleanup();
        return 6;
    }

    std::string error;
    if (!Exchange(client, "(car 5)", error) || error.rfind("error:", 0) != 0)
    {
        std::fprintf(stderr, "expected canonical error, got '%s'\n", error.c_str());
        closesocket(client);
        FreeLibrary(h);
        WSACleanup();
        return 7;
    }
    if (!RequireReply(client, "(+ 20 22)", "42"))
    {
        closesocket(client);
        FreeLibrary(h);
        WSACleanup();
        return 8;
    }

    closesocket(client);
    FreeLibrary(h);
    WSACleanup();
    std::printf("persistent canonical REPL witness OK\n");
    return 0;
}
