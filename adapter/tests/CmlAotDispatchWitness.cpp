#include "CmlAotDispatch.hpp"

#include <iostream>
#include <string>

namespace
{
struct HostState
{
    bool playerPresent = true;
    int logCalls = 0;
};

int Log(void* context)
{
    ++static_cast<HostState*>(context)->logCalls;
    return 0;
}

int PlayerPresent(void* context, int* out)
{
    *out = static_cast<HostState*>(context)->playerPresent ? 1 : 0;
    return 0;
}

int LogWhenPlayerPresent(const cml_aot::HostV1* host, void* context, cml_aot::ResultV1* out)
{
    int present = 0;
    if (host->playerPresent(context, &present) != 0)
    {
        return 1;
    }
    if (present && host->log(context) != 0)
    {
        return 1;
    }
    *out = cml_aot::ResultV1::Nil();
    return 0;
}

int StaySilent(const cml_aot::HostV1*, void*, cml_aot::ResultV1* out)
{
    *out = cml_aot::ResultV1::Nil();
    return 0;
}
} // namespace

int main()
{
    const cml_aot::HostV1 host{
        .abiVersion = cml_aot::kHostAbiVersion,
        .log = &Log,
        .playerPresent = &PlayerPresent,
    };

    HostState state;
    cml_aot::ResultV1 result{};
    if (!cml_aot::Run(&LogWhenPlayerPresent, host, &state, result) || state.logCalls != 1)
    {
        std::cerr << "logging artifact did not invoke host log exactly once\n";
        return 1;
    }
    if (!cml_aot::Run(&StaySilent, host, &state, result) || state.logCalls != 1)
    {
        std::cerr << "silent artifact changed host behavior\n";
        return 2;
    }

    const auto incompatible = cml_aot::HostV1{
        .abiVersion = cml_aot::kHostAbiVersion + 1,
        .log = &Log,
        .playerPresent = &PlayerPresent,
    };
    if (cml_aot::Run(&StaySilent, incompatible, &state, result))
    {
        std::cerr << "accepted incompatible CML host ABI\n";
        return 3;
    }
    return 0;
}
