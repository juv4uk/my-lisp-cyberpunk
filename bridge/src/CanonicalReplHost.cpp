#include "CanonicalReplHost.hpp"

#include <utility>

namespace cyberpunk_bridge
{
namespace
{
std::wstring AdjacentPath(HMODULE ownerModule, const wchar_t* fileName)
{
    wchar_t modulePath[MAX_PATH] = {};
    const DWORD length = GetModuleFileNameW(ownerModule, modulePath, MAX_PATH);
    if (length == 0 || length == MAX_PATH)
    {
        return {};
    }

    std::wstring path(modulePath, length);
    const auto slash = path.find_last_of(L"\\/");
    if (slash == std::wstring::npos)
    {
        return {};
    }
    path.resize(slash + 1);
    path += fileName;
    return path;
}

template <typename Fn>
Fn Resolve(HMODULE module, const char* name)
{
    return reinterpret_cast<Fn>(GetProcAddress(module, name));
}
} // namespace

CanonicalReplHost::~CanonicalReplHost()
{
    Stop();
}

bool CanonicalReplHost::Start(HMODULE ownerModule)
{
    Stop();

    const std::wstring embedPath = AdjacentPath(ownerModule, L"my_lisp_embed.dll");
    if (embedPath.empty())
    {
        return false;
    }

    m_embedModule = LoadLibraryW(embedPath.c_str());
    if (m_embedModule == nullptr)
    {
        return false;
    }

    m_abiVersionFn = Resolve<AbiVersionFn>(m_embedModule, "my_lisp_embed_abi_version");
    m_sessionNew = Resolve<SessionNewFn>(m_embedModule, "my_lisp_embed_session_new");
    m_eval = Resolve<EvalFn>(m_embedModule, "my_lisp_embed_eval");
    m_freeString = Resolve<FreeStringFn>(m_embedModule, "my_lisp_embed_free_string");
    m_sessionFree = Resolve<SessionFreeFn>(m_embedModule, "my_lisp_embed_session_free");

    if (m_abiVersionFn == nullptr || m_sessionNew == nullptr || m_eval == nullptr ||
        m_freeString == nullptr || m_sessionFree == nullptr)
    {
        Stop();
        return false;
    }

    m_abiVersion = m_abiVersionFn();
    if (m_abiVersion != MY_LISP_EMBED_ABI_VERSION)
    {
        Stop();
        return false;
    }

    m_session = m_sessionNew();
    if (m_session == nullptr)
    {
        Stop();
        return false;
    }
    return true;
}

std::string CanonicalReplHost::Evaluate(const std::string& source)
{
    if (m_session == nullptr || m_eval == nullptr || m_freeString == nullptr)
    {
        return "error: canonical my-lisp session is not running";
    }

    char* raw = m_eval(m_session, source.c_str());
    if (raw == nullptr)
    {
        return "error: canonical my-lisp embed returned no result";
    }

    std::string result(raw);
    m_freeString(raw);
    return result;
}

void CanonicalReplHost::Stop()
{
    if (m_session != nullptr && m_sessionFree != nullptr)
    {
        m_sessionFree(m_session);
    }
    m_session = nullptr;

    m_abiVersionFn = nullptr;
    m_sessionNew = nullptr;
    m_eval = nullptr;
    m_freeString = nullptr;
    m_sessionFree = nullptr;
    m_abiVersion = 0;

    if (m_embedModule != nullptr)
    {
        FreeLibrary(m_embedModule);
        m_embedModule = nullptr;
    }
}

} // namespace cyberpunk_bridge
