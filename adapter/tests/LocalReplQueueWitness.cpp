#include "LocalReplQueue.hpp"

#include <iostream>
#include <string>

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

    for (std::size_t i = 0; i < local_repl::kMaxPendingRequests; ++i)
    {
        const auto request = queue.Pop();
        if (!request || request->source != std::to_string(i))
        {
            std::cerr << "queue did not preserve FIFO ordering\n";
            return 4;
        }
    }
    return queue.Pop().has_value() ? 5 : 0;
}
