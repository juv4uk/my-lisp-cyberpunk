#pragma once

#include "LocalReplQueue.hpp"

#include <cstdint>
#include <functional>
#include <memory>
#include <string>

namespace local_repl
{
constexpr std::uint16_t kDefaultPort = 40777;

class LocalReplServer
{
public:
    class Impl;
    using Evaluate = std::function<std::string(const std::string&)>;

    LocalReplServer();
    ~LocalReplServer();
    LocalReplServer(const LocalReplServer&) = delete;
    LocalReplServer& operator=(const LocalReplServer&) = delete;

    [[nodiscard]] bool Start(RequestQueue& queue, std::uint16_t port = kDefaultPort);
    void Stop();
    void Drain(const Evaluate& evaluate);

private:
    std::unique_ptr<Impl> m_impl;
};
} // namespace local_repl
