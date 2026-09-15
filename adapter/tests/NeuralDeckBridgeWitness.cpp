#include "NeuralDeckBridge.hpp"
#include <iostream>
#include <stdexcept>
#include <string>

void Check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }

// A test double of the engine boundary, not a second implementation of the bridge.
struct Engine {
    int failure = 0, lookups = 0, invocations = 0;

    bool HasRtti() { ++lookups; return failure != 1; }
    bool HasToggleFunction() { return failure != 2; }
    bool InvokeToggle() { ++invocations; return failure != 3; }
};

int main() {
    try {
        const char* reasons[] = {
            "submitted: ToggleFromNative call succeeded",
            "rejected: RTTI unavailable",
            "rejected: NeuralDeckService.ToggleFromNative missing",
            "rejected: ToggleFromNative execution failed"
        };
        for (int failure = 0; failure <= 3; ++failure) {
            Engine engine;
            engine.failure = failure;
            Check(std::string(neuraldeck::QueueToggle(engine)) == reasons[failure], "wrong failure reason");
            Check(engine.invocations == ((failure == 0 || failure == 3) ? 1 : 0), "invocation after failed precondition");
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
        Check(engine.invocations == 2, "wrong number of events");
        Engine failed;
        failed.failure = 1;
        held = false;
        Check(neuraldeck::PollToggle(failed, true, held) != nullptr, "failure not reported");
        failed.failure = 0;
        Check(neuraldeck::PollToggle(failed, true, held) == nullptr, "held failure retried every frame");
        neuraldeck::PollToggle(failed, false, held);
        neuraldeck::PollToggle(failed, true, held);
        Check(failed.invocations == 1, "new press did not recover");
        std::cout << "PASS: bridge failures, invocation count, and key edges\n";
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
