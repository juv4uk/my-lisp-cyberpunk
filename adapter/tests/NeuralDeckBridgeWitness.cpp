#include "NeuralDeckBridge.hpp"
#include <cstring>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

void Check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }

struct RttiNameProbe {
    const char* requested = nullptr;
    void* GetClass(const char* name) {
        requested = name;
        return std::strcmp(name, "gameuiGameSystemUI") == 0 ? this : nullptr;
    }
};

bool VerifyUiSystemRttiLookup() {
    RttiNameProbe probe;
    return neuraldeck::LookupUiSystemClass(probe) == &probe &&
           std::strcmp(probe.requested, "gameuiGameSystemUI") == 0;
}

// A test double of the engine boundary, not a second implementation of the bridge.
struct Engine {
    using Handle = std::shared_ptr<int>;
    struct Method { void* returnType = nullptr; } method;
    struct Class {
        Engine* owner;
        Method* GetFunction(const char* name) {
            Check(std::string(name) == "QueueEvent", "wrong engine method");
            return owner->failure == 4 ? nullptr : &owner->method;
        }
    } eventClass{this}, uiClass{this};
    struct Registry {
        Engine* owner;
        Class* GetClass(const char* name) {
            if (std::string(name) == "NeuralDeckToggleEvent")
                return owner->failure == 2 ? nullptr : &owner->eventClass;
            Check(std::string(name) == neuraldeck::kUiSystemRttiName, "wrong engine class");
            return owner->failure == 3 ? nullptr : &owner->uiClass;
        }
    } registry{this};
    int failure = 0, lookups = 0, allocations = 0, submissions = 0;
    std::weak_ptr<int> observedEvent;
    Registry* Rtti() { ++lookups; return failure == 1 ? nullptr : &registry; }
    bool GetUiSystem(Handle& ui) {
        if (failure == 5) return false;
        if (failure != 6) ui = std::make_shared<int>(42);
        return true;
    }
    Handle CreateEvent(Class* type) {
        Check(type == &eventClass, "wrong event type allocated");
        ++allocations;
        if (failure == 7) return {};
        Handle event = std::make_shared<int>(7);
        observedEvent = event;
        return event;
    }
    bool Queue(const Handle& ui, Method* function, Handle& event) {
        Check(ui && *ui == 42, "wrong UI instance");
        Check(function == &method, "wrong queue function");
        Check(event && *event == 7, "event lost before submission");
        ++submissions;
        return failure != 9;
    }

    Registry* rtti = nullptr;
    Class* selectedEventClass = nullptr;
    Class* selectedUiClass = nullptr;
    Method* queueEvent = nullptr;
    Handle uiSystem;
    Handle event;

    bool HasRtti() { rtti = Rtti(); return rtti != nullptr; }
    bool HasToggleEventClass() { selectedEventClass = rtti->GetClass("NeuralDeckToggleEvent"); return selectedEventClass != nullptr; }
    bool HasUiSystemClass() { selectedUiClass = neuraldeck::LookupUiSystemClass(*rtti); return selectedUiClass != nullptr; }
    bool HasQueueEventMethod() { queueEvent = selectedUiClass->GetFunction("QueueEvent"); return queueEvent != nullptr; }
    bool GetUiSystem() { return GetUiSystem(uiSystem); }
    bool HasUiSystemHandle() const { return static_cast<bool>(uiSystem); }
    bool CreateToggleEvent() { event = CreateEvent(selectedEventClass); return static_cast<bool>(event); }
    bool QueueEventReturnsVoid() const { return queueEvent->returnType == nullptr; }
    bool SubmitToggleEvent() { return Queue(uiSystem, queueEvent, event); }
    void ReleaseToggleEvent() { event.reset(); }
};

int main() {
    try {
        Check(VerifyUiSystemRttiLookup(), "NeuralDeck UISystem RTTI lookup used the wrong class name");
        const char* reasons[] = {
            "submitted: QueueEvent call succeeded; UI receipt not yet confirmed",
            "rejected: RTTI unavailable",
            "rejected: NeuralDeckToggleEvent class missing",
            "rejected: UISystem class missing",
            "rejected: UISystem.QueueEvent method missing",
            "rejected: GetUISystem execution failed",
            "rejected: GetUISystem returned empty handle",
            "rejected: event allocation failed",
            "rejected: QueueEvent has unexpected non-void return type",
            "rejected: QueueEvent execution failed"
        };
        for (int failure = 0; failure <= 9; ++failure) {
            Engine engine;
            engine.failure = failure;
            if (failure == 8) engine.method.returnType = &engine;
            Check(std::string(neuraldeck::QueueToggle(engine)) == reasons[failure], "wrong failure reason");
            Check(engine.submissions == ((failure == 0 || failure == 9) ? 1 : 0), "submission after failed precondition");
            Check(engine.observedEvent.expired(), "bridge leaked its event ownership");
        }
        Engine engine;
        bool held = false;
        Check(neuraldeck::PollToggle(engine, false, held) == nullptr, "idle submitted event");
        Check(engine.lookups == 0, "idle touched engine");
        Check(neuraldeck::PollToggle(engine, true, held) != nullptr, "press lost");
        for (int i = 0; i < 120; ++i)
            Check(neuraldeck::PollToggle(engine, true, held) == nullptr, "held key repeated");
        Check(neuraldeck::PollToggle(engine, false, held) == nullptr, "release submitted");
        Check(neuraldeck::PollToggle(engine, true, held) != nullptr, "second press lost");
        Check(engine.submissions == 2, "wrong number of events");
        Engine failed;
        failed.failure = 1;
        held = false;
        Check(neuraldeck::PollToggle(failed, true, held) != nullptr, "failure not reported");
        failed.failure = 0;
        Check(neuraldeck::PollToggle(failed, true, held) == nullptr, "held failure retried every frame");
        neuraldeck::PollToggle(failed, false, held);
        neuraldeck::PollToggle(failed, true, held);
        Check(failed.submissions == 1, "new press did not recover");
        std::cout << "PASS: bridge failures, payload, lifetime, key edges and recovery\n";
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
