#pragma once

#include "LocalReplQueue.hpp"
#include "NeuralDeckState.hpp"

#include <string>

namespace neuraldeck
{
// Connects UI state to the existing game-thread REPL queue. It does not parse
// or evaluate source: the reply is recorded only after Running performs the
// canonical session evaluation.
class ReplBridge
{
public:
    [[nodiscard]] bool Submit(State& deck, local_repl::RequestQueue& queue)
    {
        std::string source = deck.TakeInput();
        if (source.empty())
        {
            return false;
        }
        std::string transcriptSource = source;
        return queue.Push(std::move(source), [&deck, source = std::move(transcriptSource)](std::string result) mutable {
            deck.Record(std::move(source), std::move(result));
        });
    }
};
} // namespace neuraldeck
