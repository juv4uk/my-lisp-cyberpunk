#pragma once

namespace neuraldeck
{

template <typename Engine>
const char* QueueToggle(Engine& engine)
{
    if (!engine.HasRtti()) return "rejected: RTTI unavailable";
    if (!engine.HasToggleEventClass()) return "rejected: NeuralDeckToggleEvent class missing";
    if (!engine.HasUiSystemClass()) return "rejected: UISystem class missing";
    if (!engine.HasQueueEventMethod()) return "rejected: UISystem.QueueEvent method missing";
    if (!engine.GetUiSystem()) return "rejected: GetUISystem execution failed";
    if (!engine.HasUiSystemHandle()) return "rejected: GetUISystem returned empty handle";
    if (!engine.CreateToggleEvent()) return "rejected: event allocation failed";
    if (!engine.QueueEventReturnsVoid()) {
        engine.ReleaseToggleEvent();
        return "rejected: QueueEvent has unexpected non-void return type";
    }
    const bool submitted = engine.SubmitToggleEvent();
    engine.ReleaseToggleEvent();
    return submitted
        ? "submitted: QueueEvent call succeeded; UI receipt not yet confirmed"
        : "rejected: QueueEvent execution failed";
}

template <typename Engine>
const char* PollToggle(Engine& engine, bool f10Down, bool& wasDown)
{
    const bool pressed = f10Down && !wasDown;
    wasDown = f10Down;
    return pressed ? QueueToggle(engine) : nullptr;
}

} // namespace neuraldeck
