#pragma once

// Equal surface spellings (owner 2026-09-10):
//   .my ↔ .мій   .wsm ↔ .всм   .lisp ↔ .лісп
// File/surface only — no new semantics, no mass rename.

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace surface_ext
{

inline bool ReadFileIfExists(const std::filesystem::path& path, std::string& out)
{
    std::ifstream source(path, std::ios::binary);
    if (!source)
    {
        return false;
    }
    out.assign(std::istreambuf_iterator<char>(source), std::istreambuf_iterator<char>());
    return !out.empty();
}

// Try preferred path, then the equal-spelling twin extension if any.
// Returns true and fills out + usedPath when a readable non-empty file is found.
inline bool ResolveAndRead(const std::filesystem::path& preferred, std::string& out,
                           std::filesystem::path* usedPath = nullptr)
{
    if (ReadFileIfExists(preferred, out))
    {
        if (usedPath != nullptr)
        {
            *usedPath = preferred;
        }
        return true;
    }

    const auto ext = preferred.extension().string();
    std::filesystem::path twin;
    if (ext == ".my")
    {
        twin = preferred;
        twin.replace_extension(u8".мій");
    }
    else if (ext == u8".мій")
    {
        twin = preferred;
        twin.replace_extension(".my");
    }
    else if (ext == ".wsm")
    {
        twin = preferred;
        twin.replace_extension(u8".всм");
    }
    else if (ext == u8".всм")
    {
        twin = preferred;
        twin.replace_extension(".wsm");
    }
    else if (ext == ".lisp")
    {
        twin = preferred;
        twin.replace_extension(u8".лісп");
    }
    else if (ext == u8".лісп")
    {
        twin = preferred;
        twin.replace_extension(".lisp");
    }
    else
    {
        return false;
    }

    if (ReadFileIfExists(twin, out))
    {
        if (usedPath != nullptr)
        {
            *usedPath = twin;
        }
        return true;
    }
    return false;
}

// Dispatch loader candidates: Cyrillic first (current product spelling), then Latin twin.
inline std::vector<std::filesystem::path> DispatchCandidates(const std::filesystem::path& scriptsDir)
{
    return {
        scriptsDir / u8"диспетчер.мій",
        scriptsDir / u8"диспетчер.my",
        scriptsDir / "dispatcher.my",
        scriptsDir / u8"dispatcher.мій",
    };
}

inline bool LoadFirstExisting(const std::vector<std::filesystem::path>& candidates, std::string& out,
                              std::filesystem::path* usedPath = nullptr)
{
    for (const auto& path : candidates)
    {
        if (ReadFileIfExists(path, out))
        {
            if (usedPath != nullptr)
            {
                *usedPath = path;
            }
            return true;
        }
    }
    return false;
}

} // namespace surface_ext
