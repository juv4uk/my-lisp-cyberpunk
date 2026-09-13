#pragma once

#include <cstddef>
#include <deque>
#include <string>
#include <utility>
#include <vector>

namespace neuraldeck
{
constexpr std::size_t kMaxHistoryEntries = 128;
constexpr std::size_t kMaxTranscriptEntries = 512;

struct TranscriptEntry
{
    std::size_t sequence;
    std::string source;
    std::string result;
};

// UI-only model for the in-game NeuralDeck. Input hooks and ink widgets may
// mutate this state, but evaluation remains outside it on the game thread.
class State
{
public:
    void Toggle() noexcept { m_open = !m_open; }
    void Close() noexcept { m_open = false; }
    [[nodiscard]] bool IsOpen() const noexcept { return m_open; }

    void SetInput(std::string source) { m_input = std::move(source); }
    [[nodiscard]] const std::string& Input() const noexcept { return m_input; }

    [[nodiscard]] std::string TakeInput()
    {
        std::string source = std::move(m_input);
        m_input.clear();
        if (!source.empty())
        {
            Remember(source);
        }
        return source;
    }

    [[nodiscard]] const std::vector<std::string>& History() const noexcept { return m_history; }
    [[nodiscard]] const std::deque<TranscriptEntry>& Transcript() const noexcept { return m_transcript; }

    void Record(std::string source, std::string result)
    {
        if (m_transcript.size() == kMaxTranscriptEntries)
        {
            m_transcript.pop_front();
        }
        m_transcript.push_back(TranscriptEntry{m_nextSequence++, std::move(source), std::move(result)});
    }

    void ClearTranscript() noexcept { m_transcript.clear(); }

private:
    void Remember(const std::string& source)
    {
        if (m_history.empty() || m_history.back() != source)
        {
            if (m_history.size() == kMaxHistoryEntries)
            {
                m_history.erase(m_history.begin());
            }
            m_history.push_back(source);
        }
    }

    bool m_open = false;
    std::string m_input;
    std::vector<std::string> m_history;
    std::deque<TranscriptEntry> m_transcript;
    std::size_t m_nextSequence = 1;
};
} // namespace neuraldeck
