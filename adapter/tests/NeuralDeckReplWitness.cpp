#include "NeuralDeckRepl.hpp"

#include <iostream>

int main()
{
    neuraldeck::State deck;
    neuraldeck::ReplBridge bridge;
    local_repl::RequestQueue queue;

    deck.SetInput("(визначити відповідь 42)");
    if (!bridge.Submit(deck, queue) || queue.Pending() != 1 || !deck.Transcript().empty())
    {
        return 1;
    }
    auto request = queue.Pop();
    if (!request || request->source != "(визначити відповідь 42)")
    {
        return 2;
    }
    request->reply("42");
    if (deck.Transcript().size() != 1 || deck.Transcript().front().source != "(визначити відповідь 42)" || deck.Transcript().front().result != "42")
    {
        return 3;
    }
    return bridge.Submit(deck, queue) ? 4 : 0;
}
