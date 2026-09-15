#pragma once

namespace neuraldeck
{
inline constexpr const char* kToggleClassName = "NeuralDeckService";
inline constexpr const char* kToggleFunctionName = "ToggleFromNative";

template <typename Engine>
const char* QueueToggle(Engine& engine)
{
    if (!engine.HasRtti()) return "rejected: RTTI unavailable";
    if (!engine.HasToggleFunction()) return "rejected: NeuralDeckService.ToggleFromNative missing";
    return engine.InvokeToggle()
        ? "submitted: ToggleFromNative call succeeded"
        : "rejected: ToggleFromNative execution failed";
}

template <typename Engine>
const char* PollToggle(Engine& engine, bool f10Down, bool& wasDown)
{
    const bool pressed = f10Down && !wasDown;
    wasDown = f10Down;
    return pressed ? QueueToggle(engine) : nullptr;
}

} // namespace neuraldeck
