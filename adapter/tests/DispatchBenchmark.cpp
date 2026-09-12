// Діагностичний benchmark fixed `.my` dispatch.
// Не RED4ext benchmark: тут capability stub не робить I/O, щоб відокремити
// dispatch від фізичного логування та реального frame budget.

#include <windows.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

namespace
{
using Session = void;
constexpr uint64_t kWordNil = 1;
constexpr uint64_t kWordTrue = 2;
constexpr std::size_t kWarmupIterations = 1'000;
constexpr std::size_t kSeriesCount = 30;
constexpr std::size_t kIterationsPerSeries = 10'000;

using WsmSessionInitFn = Session* (*)();
using WsmSessionFreeFn = void (*)(Session*);
using WsmHostPrimitiveFn = int32_t (*)(std::size_t argc, const uint64_t* argv, uint64_t* out);
using WsmRegisterPrimitiveFn = int32_t (*)(Session*, const char*, WsmHostPrimitiveFn);
using WsmEvalStringFn = char* (*)(Session*, const char*);
using WsmFreeStringFn = void (*)(char*);

struct WsmApi
{
    WsmSessionInitFn sessionInit = nullptr;
    WsmSessionFreeFn sessionFree = nullptr;
    WsmRegisterPrimitiveFn registerPrimitive = nullptr;
    WsmEvalStringFn evalString = nullptr;
    WsmFreeStringFn freeString = nullptr;
};

struct BenchmarkCase
{
    const char* name;
    std::filesystem::path sourcePath;
    bool playerPresent;
    std::size_t expectedLogCalls;
};

bool g_playerPresent = false;
std::size_t g_logCalls = 0;

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
    *out = g_playerPresent ? kWordTrue : kWordNil;
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

bool EvalOnce(const WsmApi& api, Session* session, const std::string& source)
{
    char* result = api.evalString(session, source.c_str());
    if (result == nullptr)
    {
        return false;
    }
    api.freeString(result);
    return true;
}

bool Preflight(const WsmApi& api, Session* session, const std::string& source, std::size_t expectedLogCalls)
{
    g_logCalls = 0;
    char* result = api.evalString(session, source.c_str());
    const bool correct = result != nullptr && std::string(result) == "()" && g_logCalls == expectedLogCalls;
    if (result != nullptr)
    {
        api.freeString(result);
    }
    return correct;
}

bool LoadApi(HMODULE module, WsmApi& api)
{
    api.sessionInit = reinterpret_cast<WsmSessionInitFn>(GetProcAddress(module, "wsm_session_init"));
    api.sessionFree = reinterpret_cast<WsmSessionFreeFn>(GetProcAddress(module, "wsm_session_free"));
    api.registerPrimitive =
        reinterpret_cast<WsmRegisterPrimitiveFn>(GetProcAddress(module, "wsm_register_primitive"));
    api.evalString = reinterpret_cast<WsmEvalStringFn>(GetProcAddress(module, "wsm_eval_string"));
    api.freeString = reinterpret_cast<WsmFreeStringFn>(GetProcAddress(module, "wsm_free_string"));
    return api.sessionInit != nullptr && api.sessionFree != nullptr && api.registerPrimitive != nullptr &&
           api.evalString != nullptr && api.freeString != nullptr;
}

bool RunCase(const WsmApi& api, const BenchmarkCase& benchmark)
{
    const std::string source = ReadSource(benchmark.sourcePath);
    if (source.empty())
    {
        std::cerr << "benchmark: cannot read " << benchmark.sourcePath.string() << "\n";
        return false;
    }

    Session* session = api.sessionInit();
    if (session == nullptr || api.registerPrimitive(session, "запиши-лог", &LogPrimitive) != 0 ||
        api.registerPrimitive(session, "гравець-присутній?", &PlayerPresentPrimitive) != 0)
    {
        if (session != nullptr)
        {
            api.sessionFree(session);
        }
        std::cerr << "benchmark: session or capability setup failed for " << benchmark.name << "\n";
        return false;
    }

    g_playerPresent = benchmark.playerPresent;
    if (!Preflight(api, session, source, benchmark.expectedLogCalls))
    {
        api.sessionFree(session);
        std::cerr << "benchmark: correctness preflight failed for " << benchmark.name << "\n";
        return false;
    }

    g_logCalls = 0;
    for (std::size_t i = 0; i < kWarmupIterations; ++i)
    {
        if (!EvalOnce(api, session, source))
        {
            api.sessionFree(session);
            std::cerr << "benchmark: warmup failed for " << benchmark.name << "\n";
            return false;
        }
    }

    std::vector<double> samples;
    samples.reserve(kSeriesCount);
    for (std::size_t series = 0; series < kSeriesCount; ++series)
    {
        const auto started = std::chrono::steady_clock::now();
        for (std::size_t i = 0; i < kIterationsPerSeries; ++i)
        {
            if (!EvalOnce(api, session, source))
            {
                api.sessionFree(session);
                std::cerr << "benchmark: timed dispatch failed for " << benchmark.name << "\n";
                return false;
            }
        }
        const auto elapsed = std::chrono::steady_clock::now() - started;
        samples.push_back(static_cast<double>(std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed).count()) /
                          static_cast<double>(kIterationsPerSeries));
    }

    const std::size_t expectedCalls =
        benchmark.expectedLogCalls * (kWarmupIterations + kSeriesCount * kIterationsPerSeries);
    if (g_logCalls != expectedCalls)
    {
        api.sessionFree(session);
        std::cerr << "benchmark: capability count drift for " << benchmark.name << ": expected " << expectedCalls
                  << ", got " << g_logCalls << "\n";
        return false;
    }
    api.sessionFree(session);

    std::sort(samples.begin(), samples.end());
    const double median = (samples[14] + samples[15]) / 2.0;
    const double p95 = samples[static_cast<std::size_t>(std::ceil(kSeriesCount * 0.95)) - 1];
    const double worst = samples.back();
    std::cout << "BENCH_RESULT\tadapter\t" << benchmark.name << "/median-ns\t" << median << "\n";
    std::cout << "BENCH_RESULT\tadapter\t" << benchmark.name << "/p95-ns\t" << p95 << "\n";
    std::cout << "BENCH_RESULT\tadapter\t" << benchmark.name << "/worst-ns\t" << worst << "\n";
    return true;
}
} // namespace

int main(int argc, char** argv)
{
    if (argc != 3)
    {
        std::cerr << "usage: dispatch-benchmark <wsm-dll> <scripts-directory>\n";
        return 2;
    }

    HMODULE module = LoadLibraryA(argv[1]);
    if (module == nullptr)
    {
        std::cerr << "benchmark: cannot load WSM DLL\n";
        return 3;
    }

    WsmApi api;
    if (!LoadApi(module, api))
    {
        FreeLibrary(module);
        std::cerr << "benchmark: WSM ABI export missing\n";
        return 4;
    }

    const std::filesystem::path scriptsDirectory(argv[2]);
    const std::vector<BenchmarkCase> cases = {
        {"dispatch-empty", scriptsDirectory / u8"бенчмарк-диспетчер-порожньо.мій", false, 0},
        {"dispatch-fact-false", scriptsDirectory / u8"бенчмарк-диспетчер-гравець-відсутній.мій", false, 0},
        {"dispatch-fact-true-noop", scriptsDirectory / u8"бенчмарк-диспетчер-гравець-присутній-тиша.мій", true,
         0},
        {"dispatch-fact-true-log", scriptsDirectory / u8"бенчмарк-диспетчер-гравець-присутній-лог.мій", true, 1},
    };

    std::cout << "BENCH_CONTEXT\twarmup=" << kWarmupIterations << "\tseries=" << kSeriesCount
              << "\titerations=" << kIterationsPerSeries << "\n";
    bool success = true;
    for (const auto& benchmark : cases)
    {
        success = RunCase(api, benchmark) && success;
    }
    FreeLibrary(module);
    return success ? 0 : 1;
}
