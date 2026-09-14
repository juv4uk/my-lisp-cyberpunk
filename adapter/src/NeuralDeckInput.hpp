#pragma once

#include "NeuralDeckState.hpp"

namespace neuraldeck
{
// The RED4ext input hook maps physical keys to this tiny UI-only controller.
// It owns no game policy and does not pause or otherwise alter the game.
enum class Key
{
    F10,
    Tab,
};

class InputController
{
public:
    // Returns true only when NeuralDeck consumes the F10 press. Tab remains a
    // game key even while the deck is visible.
    [[nodiscard]] bool HandleKey(Key key, bool pressed, State& deck) noexcept
    {
        if (key != Key::F10)
        {
            return false;
        }

        if (!pressed)
        {
            m_f10Down = false;
            return false;
        }

        if (m_f10Down)
        {
            return false;
        }

        m_f10Down = true;
        deck.Toggle();
        return true;
    }

private:
    bool m_f10Down = false;
};
} // namespace neuraldeck
