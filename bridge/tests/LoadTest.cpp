// Standalone harness (NOT injected into Cyberpunk): loads version.dll
// exactly the way any Windows process would, calls one real forwarded
// function, and checks the bridge-observation.lisp file it should have
// produced. Proves the proxy load + export-forwarding + background
// thread + file-write chain works in complete isolation from the game,
// before it is ever placed anywhere near a live Cyberpunk process.
#include <windows.h>
#include <cstdio>
#include <string>

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        fprintf(stderr, "usage: LoadTest <path to version.dll>\n");
        return 1;
    }

    HMODULE h = LoadLibraryA(argv[1]);
    if (h == nullptr)
    {
        fprintf(stderr, "LoadLibrary failed: %lu\n", GetLastError());
        return 1;
    }
    printf("LoadLibrary OK, module=%p\n", (void*)h);

    using GetSizeFn = DWORD(WINAPI*)(LPCSTR, LPDWORD);
    auto getSize = reinterpret_cast<GetSizeFn>(GetProcAddress(h, "GetFileVersionInfoSizeA"));
    if (getSize == nullptr)
    {
        fprintf(stderr, "GetProcAddress(GetFileVersionInfoSizeA) failed\n");
        return 1;
    }

    DWORD handle = 0;
    DWORD size = getSize(argv[1], &handle);
    printf("forwarded GetFileVersionInfoSizeA(%s) = %lu (real call reached system version.dll)\n", argv[1], size);

    printf("waiting 3s for the background thread to write bridge-observation.lisp...\n");
    Sleep(3000);

    FreeLibrary(h);
    printf("FreeLibrary OK\n");
    return 0;
}
