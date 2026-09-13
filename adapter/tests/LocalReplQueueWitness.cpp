#include "LocalReplQueue.hpp"

#include <iostream>
#include <string>
#include <vector>

int main()
{
    local_repl::RequestQueue queue;
    const auto reply = [](std::string) {};

    if (queue.Push(std::string(local_repl::kMaxRequestBytes + 1, 'x'), reply))
    {
        std::cerr << "accepted oversized request\n";
        return 1;
    }
    for (std::size_t i = 0; i < local_repl::kMaxPendingRequests; ++i)
    {
        if (!queue.Push(std::to_string(i), reply))
        {
            std::cerr << "rejected request before queue capacity\n";
            return 2;
        }
    }
    if (queue.Push("overflow", reply))
    {
        std::cerr << "accepted request beyond queue capacity\n";
        return 3;
    }

    std::vector<std::string> cancelled;
    local_repl::RequestQueue unloadQueue;
    if (!unloadQueue.Push("first", [&cancelled](std::string result) { cancelled.push_back(std::move(result)); }) ||
        !unloadQueue.Push("second", [&cancelled](std::string result) { cancelled.push_back(std::move(result)); }))
    {
        std::cerr << "could not prepare pending unload requests\n";
        return 4;
    }
    if (unloadQueue.CancelAll("error: REPL session ended") != 2 || unloadQueue.Pending() != 0 ||
        cancelled != std::vector<std::string>{"error: REPL session ended", "error: REPL session ended"})
    {
        std::cerr << "pending requests survived session cancellation\n";
        return 5;
    }

    for (std::size_t i = 0; i < local_repl::kMaxPendingRequests; ++i)
    {
        const auto request = queue.Pop();
        if (!request || request->source != std::to_string(i))
        {
            std::cerr << "queue did not preserve FIFO ordering\n";
            return 6;
        }
    }
    return queue.Pop().has_value() ? 7 : 0;
}
