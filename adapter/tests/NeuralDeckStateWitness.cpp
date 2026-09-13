#include "NeuralDeckState.hpp"

#include <iostream>

int main()
{
    neuraldeck::State deck;
    if (deck.IsOpen()) return 1;
    deck.Toggle();
    if (!deck.IsOpen()) return 2;

    deck.SetInput("(визначити відповідь 42)");
    const auto source = deck.TakeInput();
    if (source != "(визначити відповідь 42)" || !deck.Input().empty()) return 3;
    deck.Record(source, "42");
    if (deck.History().size() != 1 || deck.Transcript().size() != 1) return 4;
    if (deck.Transcript().front().sequence != 1 || deck.Transcript().front().result != "42") return 5;

    deck.Close();
    deck.ClearTranscript();
    return !deck.IsOpen() && deck.Transcript().empty() ? 0 : 6;
}
