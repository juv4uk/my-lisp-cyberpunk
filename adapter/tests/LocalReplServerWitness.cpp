#include "LocalReplQueue.hpp"
#include "LocalReplServer.hpp"

#include <winsock2.h>
#include <ws2tcpip.h>

#include <chrono>
#include <iostream>
#include <string>
#include <thread>

namespace
{
bool WaitForPending(const local_repl::RequestQueue& queue)
{
    for (int attempt = 0; attempt < 100; ++attempt)
    {
        if (queue.Pending() != 0)
        {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return false;
}

bool SendAndReceive(SOCKET client, const std::string& source, local_repl::LocalReplServer& server,
                    local_repl::RequestQueue& queue, const std::string& expected)
{
    const std::string request = source + "\n";
    if (send(client, request.data(), static_cast<int>(request.size()), 0) != static_cast<int>(request.size()) ||
        !WaitForPending(queue))
    {
        return false;
    }
    server.Drain([](const std::string& form) { return "result:" + form; });
    char reply[256] = {};
    const int received = recv(client, reply, sizeof(reply) - 1, 0);
    return received > 0 && std::string(reply, static_cast<std::size_t>(received)) == expected + "\n";
}
} // namespace

int main()
{
    constexpr std::uint16_t kTestPort = 40778;
    local_repl::RequestQueue queue;
    local_repl::LocalReplServer server;
    if (!server.Start(queue, kTestPort))
    {
        std::cerr << "could not start loopback server\n";
        return 1;
    }

    SOCKET client = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(kTestPort);
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (client == INVALID_SOCKET || connect(client, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) == SOCKET_ERROR)
    {
        server.Stop();
        std::cerr << "could not connect loopback client\n";
        return 2;
    }

    const bool first = SendAndReceive(client, "(quote first)", server, queue, "result:(quote first)");
    const bool second = SendAndReceive(client, "(quote second)", server, queue, "result:(quote second)");
    closesocket(client);
    server.Stop();
    if (!first || !second)
    {
        std::cerr << "loopback request/response proof failed\n";
        return 3;
    }

    if (!server.Start(queue, kTestPort))
    {
        std::cerr << "could not restart loopback server\n";
        return 4;
    }
    std::string cancelled;
    if (!queue.Push("pending-at-unload", [&cancelled](std::string result) { cancelled = std::move(result); }))
    {
        server.Stop();
        std::cerr << "could not enqueue pending request\n";
        return 5;
    }
    server.Stop();
    if (queue.Pending() != 0 || cancelled != "error: REPL session ended")
    {
        std::cerr << "Stop left a request alive past the REPL session\n";
        return 6;
    }
    return 0;
}
