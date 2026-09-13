#pragma once

#include <cstdint>

// Consumer-side ABI for a CML-generated fixed-dispatch artifact. It carries
// host mechanisms only: CML owns the compiled Lisp control flow, while the
// adapter owns the RED4ext calls behind these function pointers.
namespace cml_aot
{
constexpr std::uint32_t kHostAbiVersion = 1;

enum class ResultKind : std::uint32_t
{
    Nil = 0,
    Text = 1,
    Error = 2,
};

struct ResultV1
{
    ResultKind kind = ResultKind::Nil;
    const char* presentation = "()";

    [[nodiscard]] static constexpr ResultV1 Nil() noexcept
    {
        return {};
    }
};

using LogFn = int (*)(void* context);
using PlayerPresentFn = int (*)(void* context, int* out);

struct HostV1
{
    std::uint32_t abiVersion = kHostAbiVersion;
    LogFn log = nullptr;
    PlayerPresentFn playerPresent = nullptr;
};

using DispatchFn = int (*)(const HostV1* host, void* context, ResultV1* out);

[[nodiscard]] inline bool Run(DispatchFn dispatch, const HostV1& host, void* context, ResultV1& out) noexcept
{
    if (dispatch == nullptr || host.abiVersion != kHostAbiVersion)
    {
        return false;
    }
    return dispatch(&host, context, &out) == 0;
}
} // namespace cml_aot
