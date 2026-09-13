#include "LocalReplServer.hpp"

#include <winsock2.h>
#include <ws2tcpip.h>

#include <atomic>
#include <chrono>
#include <future>
#include <mutex>
#include <thread>
#include <utility>

namespace local_repl
{
class LocalReplServer::Impl
{
public:
    RequestQueue* queue = nullptr;
    SOCKET listener = INVALID_SOCKET;
    std::atomic_bool running = false;
    std::thread worker;
    bool winsockReady = false;
};

namespace
{
void SendLine(SOCKET client, const std::string& text)
{
    const std::string line = text + "\n";
    const char* cursor = line.data();
    std::size_t remaining = line.size();
    while (remaining != 0)
    {
        const int sent = send(client, cursor, static_cast<int>(remaining), 0);
        if (sent <= 0)
        {
            return;
        }
        cursor += sent;
        remaining -= static_cast<std::size_t>(sent);
    }
}

void ServeClient(LocalReplServer::Impl& impl, SOCKET client)
{
    std::string buffered;
    char chunk[1024];
    while (impl.running)
    {
        const int received = recv(client, chunk, sizeof(chunk), 0);
        if (received <= 0)
        {
            return;
        }
        buffered.append(chunk, static_cast<std::size_t>(received));
        if (buffered.size() > kMaxRequestBytes && buffered.find('\n') == std::string::npos)
        {
            SendLine(client, "error: REPL request exceeds 16384 bytes");
            return;
        }
        std::size_t newline = buffered.find('\n');
        while (newline != std::string::npos)
        {
            std::string source = buffered.substr(0, newline);
            buffered.erase(0, newline + 1);
            if (!source.empty() && source.back() == '\r')
            {
                source.pop_back();
            }
            if (source.empty())
            {
                continue;
            }
            auto reply = std::make_shared<std::promise<std::string>>();
            auto future = reply->get_future();
            if (!impl.queue->Push(std::move(source), [reply](std::string result) { reply->set_value(std::move(result)); }))
            {
                SendLine(client, "error: REPL request rejected (size or queue capacity)");
                continue;
            }
            while (impl.running && future.wait_for(std::chrono::milliseconds(50)) != std::future_status::ready)
            {
            }
            if (!impl.running)
            {
                return;
            }
            SendLine(client, future.get());
            newline = buffered.find('\n');
        }
    }
}

void Run(LocalReplServer::Impl& impl)
{
    while (impl.running)
    {
        const SOCKET client = accept(impl.listener, nullptr, nullptr);
        if (client == INVALID_SOCKET)
        {
            return;
        }
        ServeClient(impl, client);
        closesocket(client);
    }
}
} // namespace

LocalReplServer::LocalReplServer()
    : m_impl(std::make_unique<Impl>())
{
}

LocalReplServer::~LocalReplServer()
{
    Stop();
}

bool LocalReplServer::Start(RequestQueue& queue, std::uint16_t port)
{
    if (m_impl->running)
    {
        return true;
    }
    WSADATA data{};
    if (WSAStartup(MAKEWORD(2, 2), &data) != 0)
    {
        return false;
    }
    m_impl->winsockReady = true;
    m_impl->listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (m_impl->listener == INVALID_SOCKET)
    {
        Stop();
        return false;
    }
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (bind(m_impl->listener, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) == SOCKET_ERROR ||
        listen(m_impl->listener, SOMAXCONN) == SOCKET_ERROR)
    {
        Stop();
        return false;
    }
    m_impl->queue = &queue;
    m_impl->running = true;
    m_impl->worker = std::thread([impl = m_impl.get()] { Run(*impl); });
    return true;
}

void LocalReplServer::Stop()
{
    if (!m_impl)
    {
        return;
    }
    m_impl->running = false;
    if (m_impl->listener != INVALID_SOCKET)
    {
        closesocket(m_impl->listener);
        m_impl->listener = INVALID_SOCKET;
    }
    if (m_impl->worker.joinable())
    {
        m_impl->worker.join();
    }
    if (m_impl->queue != nullptr)
    {
        (void)m_impl->queue->CancelAll("error: REPL session ended");
    }
    m_impl->queue = nullptr;
    if (m_impl->winsockReady)
    {
        WSACleanup();
        m_impl->winsockReady = false;
    }
}

void LocalReplServer::Drain(const Evaluate& evaluate)
{
    while (const auto request = m_impl->queue->Pop())
    {
        request->reply(evaluate(request->source));
    }
}
} // namespace local_repl
