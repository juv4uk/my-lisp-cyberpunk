// Виконуваний доказ fixed dispatch v0.
// Один і той самий C++ host повідомляє факт «гравець присутній» двом `.my`
// програмам. Лише сценарій вирішує, чи викликати capability `запиши-лог`.

#include <windows.h>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

namespace
{
using Session = void;
constexpr uint64_t kWordNil = 1;
constexpr uint64_t kWordTrue = 2;

using WsmSessionInitFn = Session* (*)();
using WsmSessionFreeFn = void (*)(Session*);
using WsmHostPrimitiveFn = int32_t (*)(std::size_t argc, const uint64_t* argv, uint64_t* out);
using WsmRegisterPrimitiveFn = int32_t (*)(Session*, const char*, WsmHostPrimitiveFn);
using WsmEvalStringFn = char* (*)(Session*, const char*);
using WsmFreeStringFn = void (*)(char*);

int g_logCalls = 0;

int32_t LogPrimitive(std::size_t argc, const uint64_t*, uint64_t* out)
{
    if (argc != 0 || out == nullptr)
    {
        return 1;
    }
    ++g_logCalls;
    *out = kWordNil;
    return 0;
}

int32_t PlayerPresentPrimitive(std::size_t argc, const uint64_t*, uint64_t* out)
{
    if (argc != 0 || out == nullptr)
    {
        return 1;
    }
    *out = kWordTrue;
    return 0;
}

std::string ReadSource(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    if (!input)
    {
        return {};
    }
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

bool RunScenario(HMODULE module, const std::filesystem::path& scenario, int expectedLogCalls)
{
    const auto sessionInit = reinterpret_cast<WsmSessionInitFn>(GetProcAddress(module, "wsm_session_init"));
    const auto sessionFree = reinterpret_cast<WsmSessionFreeFn>(GetProcAddress(module, "wsm_session_free"));
    const auto registerPrimitive =
        reinterpret_cast<WsmRegisterPrimitiveFn>(GetProcAddress(module, "wsm_register_primitive"));
    const auto evalString = reinterpret_cast<WsmEvalStringFn>(GetProcAddress(module, "wsm_eval_string"));
    const auto freeString = reinterpret_cast<WsmFreeStringFn>(GetProcAddress(module, "wsm_free_string"));
    if (sessionInit == nullptr || sessionFree == nullptr || registerPrimitive == nullptr || evalString == nullptr ||
        freeString == nullptr)
    {
        std::cerr << "witness: WSM ABI export missing\n";
        return false;
    }

    const std::string source = ReadSource(scenario);
    if (source.empty())
    {
        std::cerr << "witness: cannot read scenario " << scenario.string() << "\n";
        return false;
    }

    Session* session = sessionInit();
    if (session == nullptr)
    {
        std::cerr << "witness: session init failed\n";
        return false;
    }

    const bool registered = registerPrimitive(session, "запиши-лог", &LogPrimitive) == 0 &&
                            registerPrimitive(session, "гравець-присутній?", &PlayerPresentPrimitive) == 0;
    if (!registered)
    {
        sessionFree(session);
        std::cerr << "witness: capability registration failed\n";
        return false;
    }

    g_logCalls = 0;
    char* result = evalString(session, source.c_str());
    const bool correct = result != nullptr && std::string(result) == "()" && g_logCalls == expectedLogCalls;
    if (result != nullptr)
    {
        freeString(result);
    }
    sessionFree(session);

    if (!correct)
    {
        std::cerr << "witness: " << scenario.filename().string() << " expected " << expectedLogCalls
                  << " log calls, got " << g_logCalls << "\n";
    }
    return correct;
}
} // namespace

int main(int argc, char** argv)
{
    if (argc != 4)
    {
        std::cerr << "usage: dispatch-witness <wsm-dll> <log-scenario.my> <silent-scenario.my>\n";
        return 2;
    }

    HMODULE module = LoadLibraryA(argv[1]);
    if (module == nullptr)
    {
        std::cerr << "witness: cannot load WSM DLL\n";
        return 3;
    }

    const bool logScenario = RunScenario(module, argv[2], 1);
    const bool silentScenario = RunScenario(module, argv[3], 0);
    FreeLibrary(module);
    if (!logScenario || !silentScenario)
    {
        return 1;
    }

    std::cout << "fixed-dispatch witness: same host fact; `.my` chose 1 then 0 log calls\n";
    return 0;
}
