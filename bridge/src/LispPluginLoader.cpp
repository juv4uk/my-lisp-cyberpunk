#include "LispPluginLoader.hpp"

#include "CanonicalReplHost.hpp"

#include <bcrypt.h>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <string>
#include <system_error>
#include <vector>

namespace cyberpunk_bridge
{
namespace
{

struct LoadRecord
{
    std::string relativePath;
    std::string sha256;
    bool loaded = false;
};

std::filesystem::path ModuleDirectory(HMODULE ownerModule)
{
    wchar_t modulePath[MAX_PATH] = {};
    const DWORD length = GetModuleFileNameW(ownerModule, modulePath, MAX_PATH);
    if (length == 0 || length >= MAX_PATH)
    {
        return {};
    }
    return std::filesystem::path(std::wstring(modulePath, length)).parent_path();
}

bool IsRegularNonSymlink(const std::filesystem::path& path)
{
    std::error_code error;
    const auto status = std::filesystem::symlink_status(path, error);
    if (error)
    {
        return false;
    }
    return !std::filesystem::is_symlink(status) && std::filesystem::is_regular_file(status);
}

bool ReadExactBytes(const std::filesystem::path& path, std::string& bytes)
{
    std::ifstream input(path, std::ios::binary);
    if (!input)
    {
        return false;
    }

    std::ostringstream buffer;
    buffer << input.rdbuf();
    if (input.bad())
    {
        return false;
    }

    bytes = buffer.str();
    return true;
}

bool Sha256(const std::string& bytes, std::string& hex)
{
    BCRYPT_ALG_HANDLE algorithm = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;

    NTSTATUS status = BCryptOpenAlgorithmProvider(
        &algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0);
    if (status < 0)
    {
        return false;
    }

    DWORD objectLength = 0;
    DWORD hashLength = 0;
    DWORD returned = 0;

    status = BCryptGetProperty(
        algorithm,
        BCRYPT_OBJECT_LENGTH,
        reinterpret_cast<PUCHAR>(&objectLength),
        sizeof(objectLength),
        &returned,
        0);
    if (status < 0)
    {
        BCryptCloseAlgorithmProvider(algorithm, 0);
        return false;
    }

    status = BCryptGetProperty(
        algorithm,
        BCRYPT_HASH_LENGTH,
        reinterpret_cast<PUCHAR>(&hashLength),
        sizeof(hashLength),
        &returned,
        0);
    if (status < 0 || hashLength == 0)
    {
        BCryptCloseAlgorithmProvider(algorithm, 0);
        return false;
    }

    std::vector<UCHAR> object(objectLength);
    std::vector<UCHAR> digest(hashLength);

    status = BCryptCreateHash(
        algorithm,
        &hash,
        object.data(),
        static_cast<ULONG>(object.size()),
        nullptr,
        0,
        0);
    if (status < 0)
    {
        BCryptCloseAlgorithmProvider(algorithm, 0);
        return false;
    }

    if (bytes.size() > static_cast<std::size_t>(std::numeric_limits<ULONG>::max()))
    {
        BCryptDestroyHash(hash);
        BCryptCloseAlgorithmProvider(algorithm, 0);
        return false;
    }

    if (!bytes.empty())
    {
        status = BCryptHashData(
            hash,
            reinterpret_cast<PUCHAR>(const_cast<char*>(bytes.data())),
            static_cast<ULONG>(bytes.size()),
            0);
    }

    if (status >= 0)
    {
        status = BCryptFinishHash(
            hash,
            digest.data(),
            static_cast<ULONG>(digest.size()),
            0);
    }

    BCryptDestroyHash(hash);
    BCryptCloseAlgorithmProvider(algorithm, 0);

    if (status < 0)
    {
        return false;
    }

    std::ostringstream rendered;
    rendered << std::hex << std::setfill('0');
    for (const unsigned char byte : digest)
    {
        rendered << std::setw(2) << static_cast<unsigned int>(byte);
    }
    hex = rendered.str();
    return true;
}

std::string EscapeLispString(const std::string& text)
{
    std::string escaped;
    escaped.reserve(text.size());

    for (const unsigned char byte : text)
    {
        switch (byte)
        {
        case '\\':
            escaped += "\\\\";
            break;
        case '"':
            escaped += "\\\"";
            break;
        case '\n':
            escaped += "\\n";
            break;
        case '\r':
            escaped += "\\r";
            break;
        case '\t':
            escaped += "\\t";
            break;
        default:
            escaped.push_back(static_cast<char>(byte));
            break;
        }
    }

    return escaped;
}

std::string RelativeDisplay(
    const std::filesystem::path& pluginRoot,
    const std::filesystem::path& path)
{
    std::error_code error;
    const auto relative = std::filesystem::relative(path, pluginRoot, error);
    if (error)
    {
        return {};
    }
    return relative.generic_u8string();
}

LoadRecord EvaluateOne(
    CanonicalReplHost& host,
    const std::filesystem::path& pluginRoot,
    const std::filesystem::path& path)
{
    LoadRecord record;
    record.relativePath = RelativeDisplay(pluginRoot, path);

    std::string source;
    if (record.relativePath.empty() || !ReadExactBytes(path, source))
    {
        return record;
    }

    if (!Sha256(source, record.sha256))
    {
        return record;
    }

    const std::string response = host.Evaluate(source);
    record.loaded = response.rfind("error:", 0) != 0;
    return record;
}

bool AppendReport(
    HMODULE ownerModule,
    const std::vector<LoadRecord>& records)
{
    const auto moduleDirectory = ModuleDirectory(ownerModule);
    if (moduleDirectory.empty())
    {
        return false;
    }

    const auto observationPath = moduleDirectory / L"bridge-observation.lisp";
    std::ofstream output(observationPath, std::ios::binary | std::ios::app);
    if (!output)
    {
        return false;
    }

    output << "(plugin-load-report/1 (root \"my-lisp\") (entries";
    for (const auto& record : records)
    {
        output << "\n    ((path \""
               << EscapeLispString(record.relativePath)
               << "\") (sha256 ";
        if (record.sha256.empty())
        {
            output << "()";
        }
        else
        {
            output << "\"" << record.sha256 << "\"";
        }
        output << ") (status " << (record.loaded ? "loaded" : "error") << "))";
    }
    output << "))\n";
    output.flush();
    return output.good();
}

} // namespace

bool LispPluginLoader::LoadAndRecord(
    CanonicalReplHost& host,
    HMODULE ownerModule)
{
    const auto moduleDirectory = ModuleDirectory(ownerModule);
    if (moduleDirectory.empty())
    {
        return false;
    }

    const auto pluginRoot = moduleDirectory / L"my-lisp";
    std::error_code error;
    const auto rootStatus = std::filesystem::symlink_status(pluginRoot, error);

    std::vector<LoadRecord> records;

    if (error && error != std::errc::no_such_file_or_directory)
    {
        return false;
    }

    if (!error && std::filesystem::exists(rootStatus))
    {
        if (std::filesystem::is_symlink(rootStatus) ||
            !std::filesystem::is_directory(rootStatus))
        {
            return false;
        }

        const auto initPath = pluginRoot / L"init.lisp";
        if (IsRegularNonSymlink(initPath))
        {
            records.push_back(EvaluateOne(host, pluginRoot, initPath));
        }

        const auto pluginsDirectory = pluginRoot / L"plugins";
        error.clear();
        const auto pluginsStatus =
            std::filesystem::symlink_status(pluginsDirectory, error);

        if (error && error != std::errc::no_such_file_or_directory)
        {
            return false;
        }

        if (!error && std::filesystem::exists(pluginsStatus))
        {
            if (std::filesystem::is_symlink(pluginsStatus) ||
                !std::filesystem::is_directory(pluginsStatus))
            {
                return false;
            }

            std::vector<std::filesystem::path> pluginPaths;
            std::filesystem::directory_iterator end;
            for (std::filesystem::directory_iterator it(pluginsDirectory, error);
                 !error && it != end;
                 it.increment(error))
            {
                const auto& path = it->path();
                if (path.extension() == L".lisp" && IsRegularNonSymlink(path))
                {
                    pluginPaths.push_back(path);
                }
            }
            if (error)
            {
                return false;
            }

            std::sort(
                pluginPaths.begin(),
                pluginPaths.end(),
                [](const auto& left, const auto& right)
                {
                    return left.filename().wstring() < right.filename().wstring();
                });

            for (const auto& path : pluginPaths)
            {
                records.push_back(EvaluateOne(host, pluginRoot, path));
            }
        }
    }

    return AppendReport(ownerModule, records);
}

} // namespace cyberpunk_bridge
