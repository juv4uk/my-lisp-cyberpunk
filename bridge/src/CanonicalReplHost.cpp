#include "CanonicalReplHost.hpp"

namespace cyberpunk_bridge
{

CanonicalReplHost::~CanonicalReplHost()
{
    Stop();
}

bool CanonicalReplHost::Start()
{
    Stop();

    m_abiVersion = my_lisp_embed_abi_version();
    if (m_abiVersion != MY_LISP_EMBED_ABI_VERSION)
    {
        m_abiVersion = 0;
        return false;
    }

    m_session = my_lisp_embed_session_new();
    if (m_session == nullptr)
    {
        m_abiVersion = 0;
        return false;
    }
    return true;
}

std::string CanonicalReplHost::Evaluate(const std::string& source)
{
    if (m_session == nullptr)
    {
        return "error: canonical my-lisp session is not running";
    }

    char* raw = my_lisp_embed_eval(m_session, source.c_str());
    if (raw == nullptr)
    {
        return "error: canonical my-lisp embed returned no result";
    }

    std::string result(raw);
    my_lisp_embed_free_string(raw);
    return result;
}

void CanonicalReplHost::Stop()
{
    if (m_session != nullptr)
    {
        my_lisp_embed_session_free(m_session);
        m_session = nullptr;
    }
    m_abiVersion = 0;
}

} // namespace cyberpunk_bridge
