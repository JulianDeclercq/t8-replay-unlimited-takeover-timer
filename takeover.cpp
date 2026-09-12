// T8 Replay Unlimited Takeover Timer
//
// Tekken 8 caps the "Takeover" in My Replay & Tips at 10 seconds. The cap is a
// hard-coded 600 (frames @ 60 fps) in two places:
//
//     duration_frames = min(600, battle_subsystem->remaining_round_frames)
//
// stored at replay_controller+0x64 and decremented once per frame by the
// takeover tick; the HUD shows ceil(frames / 60). Raising both constants to
// 0xFFFFFF makes min() always pick the remaining round time, so a takeover
// simply lasts until the round clock would have run out. Nothing else needs
// patching, and the game still ends the takeover on a KO exactly as before.
//
// Found 2026-09-12; originally a Cheat Engine script that had to be re-run
// after every game start. This .asi does it at load instead.
//
// Both sites are located by AOB, never by RVA, so a game patch that moves .text
// costs nothing. The patterns live here as HEX TEXT and are decoded at runtime:
// storing them as raw opcode-byte arrays in static data trips Defender's !ml
// heuristic.

#include <windows.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cwchar>

namespace {

constexpr uint32_t kVanilla   = 600;        // 10 s @ 60 fps
constexpr uint32_t kUnlimited = 0x00FFFFFF; // ~77 hours; min() now always loses

HMODULE g_self = nullptr;

// ---------------------------------------------------------------------------
// Log: one file next to this .asi, truncated on the first line of each launch.
// Win32 file calls only, so there is no CRT file IO from a freshly loaded DLL
// and no dependence on where the game points its working directory.
// ---------------------------------------------------------------------------
void log_line(const char* text)
{
    static bool truncated = false;

    wchar_t path[MAX_PATH];
    const DWORD n = GetModuleFileNameW(g_self, path, MAX_PATH);
    if (n == 0 || n >= MAX_PATH) return;

    wchar_t* const slash = wcsrchr(path, L'\\');
    if (!slash) return;
    const size_t room = MAX_PATH - static_cast<size_t>(slash + 1 - path);
    if (wcscpy_s(slash + 1, room, L"t8_replay_unlimited_takeover_timer.log") != 0) return;

    const HANDLE h = CreateFileW(path, FILE_APPEND_DATA, FILE_SHARE_READ, nullptr,
                                 truncated ? OPEN_ALWAYS : CREATE_ALWAYS,
                                 FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return;
    truncated = true;

    DWORD written = 0;
    WriteFile(h, text, static_cast<DWORD>(strlen(text)), &written, nullptr);
    WriteFile(h, "\r\n", 2, &written, nullptr);
    CloseHandle(h);
}

// ---------------------------------------------------------------------------
// AOB scan
// ---------------------------------------------------------------------------
int nibble(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

// "41 B8 ?? 02" -> bytes plus a per-byte "must match" mask.
bool decode(const char* hex, uint8_t* bytes, bool* mask, size_t cap, size_t* out_len)
{
    size_t n = 0;
    for (const char* p = hex; *p;)
    {
        if (*p == ' ') { ++p; continue; }
        if (n >= cap) return false;

        if (*p == '?')
        {
            bytes[n] = 0;
            mask[n]  = false;
            p += (p[1] == '?') ? 2 : 1;
        }
        else
        {
            const int hi = nibble(p[0]);
            const int lo = nibble(p[1]);
            if (hi < 0 || lo < 0) return false;
            bytes[n] = static_cast<uint8_t>((hi << 4) | lo);
            mask[n]  = true;
            p += 2;
        }
        ++n;
    }
    *out_len = n;
    return n > 0;
}

// Counts matches but stops at 2: the only question is whether there is exactly
// one, and a full sweep of .text for every site is wasted work.
size_t scan(const uint8_t* begin, size_t size,
            const uint8_t* pat, const bool* mask, size_t len,
            const uint8_t** first)
{
    if (len == 0 || size < len) return 0;

    size_t hits = 0;
    for (size_t i = 0; i + len <= size; ++i)
    {
        size_t j = 0;
        for (; j < len; ++j)
            if (mask[j] && begin[i + j] != pat[j]) break;

        if (j == len)
        {
            if (hits == 0) *first = begin + i;
            if (++hits > 1) break;
        }
    }
    return hits;
}

// ---------------------------------------------------------------------------
// The two sites. imm_off = byte offset of the 32-bit 600 inside the match.
// ---------------------------------------------------------------------------
struct Site
{
    const char* name;
    const char* hex;
    size_t      imm_off;
};

const Site kSites[] = {
    // menu start path: mov r8d,258h ; cmp [rax+1Ch],r8d ; cmovl r8d,[rax+1Ch] ; mov [rbx+64h],r8d
    { "takeover start ", "41 B8 58 02 00 00 44 39 40 1C 44 0F 4C 40 1C 44 89 43 64", 2 },
    // lead-in helper:  mov ecx,[rax+1Ch] ; mov eax,258h ; cmp ecx,eax ; cmovl eax,ecx
    { "takeover helper", "8B 48 1C B8 58 02 00 00 3B C8 0F 4C C1", 4 },
};

void patch_site(const uint8_t* base, const uint8_t* text, size_t text_size, const Site& site)
{
    char    msg[256];
    uint8_t bytes[64];
    bool    mask[64];
    size_t  len = 0;

    if (!decode(site.hex, bytes, mask, sizeof(bytes), &len))
    {
        sprintf_s(msg, "%s: bad pattern (bug)", site.name);
        log_line(msg);
        return;
    }

    const uint8_t* hit  = nullptr;
    const size_t   hits = scan(text, text_size, bytes, mask, len, &hit);
    if (hits == 0)
    {
        sprintf_s(msg, "%s: NOT FOUND - signature needs re-deriving", site.name);
        log_line(msg);
        return;
    }
    if (hits > 1)
    {
        sprintf_s(msg, "%s: AMBIGUOUS (2+ matches) - not patched", site.name);
        log_line(msg);
        return;
    }

    const unsigned long long rva = static_cast<unsigned long long>(hit - base);
    uint32_t* const imm    = reinterpret_cast<uint32_t*>(const_cast<uint8_t*>(hit) + site.imm_off);
    const uint32_t  before = *imm;

    // Anything but the stock 600 (or our own value, on a second load) means the
    // match is not the instruction we think it is. Leave it alone.
    if (before != kVanilla && before != kUnlimited)
    {
        sprintf_s(msg, "%s: RVA 0x%llX unexpected immediate %u - not patched",
                  site.name, rva, before);
        log_line(msg);
        return;
    }

    DWORD old = 0;
    if (!VirtualProtect(imm, sizeof(uint32_t), PAGE_EXECUTE_READWRITE, &old))
    {
        sprintf_s(msg, "%s: RVA 0x%llX VirtualProtect failed (%lu)",
                  site.name, rva, GetLastError());
        log_line(msg);
        return;
    }
    *imm = kUnlimited;
    VirtualProtect(imm, sizeof(uint32_t), old, &old);
    FlushInstructionCache(GetCurrentProcess(), imm, sizeof(uint32_t));

    sprintf_s(msg, "%s: RVA 0x%llX  %u -> %u", site.name, rva, before, *imm);
    log_line(msg);
}

DWORD WINAPI worker(LPVOID)
{
    const uint8_t* const base = reinterpret_cast<const uint8_t*>(GetModuleHandleW(nullptr));
    if (!base) return 0;

    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    const auto* nt  = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE || nt->Signature != IMAGE_NT_SIGNATURE)
    {
        log_line("host image is not a PE - nothing patched");
        return 0;
    }

    // The build key, so a NOT FOUND line says which build stopped matching.
    char msg[256];
    sprintf_s(msg, "build: TimeDateStamp=0x%08lX SizeOfImage=0x%lX",
              nt->FileHeader.TimeDateStamp, nt->OptionalHeader.SizeOfImage);
    log_line(msg);

    const uint8_t* text      = nullptr;
    size_t         text_size = 0;

    const IMAGE_SECTION_HEADER* sec = IMAGE_FIRST_SECTION(nt);
    for (WORD i = 0; i < nt->FileHeader.NumberOfSections; ++i)
    {
        if (memcmp(sec[i].Name, ".text", 6) == 0)
        {
            text      = base + sec[i].VirtualAddress;
            text_size = sec[i].Misc.VirtualSize;
            break;
        }
    }
    if (!text || text_size == 0)
    {
        log_line("no .text section - nothing patched");
        return 0;
    }

    for (const Site& site : kSites)
        patch_site(base, text, text_size, site);

    return 0;
}

} // namespace

#ifdef T8T_DINPUT8_PROXY
// dinput8_proxy.cpp — only in the dinput8.dll build, which has to forward the
// real DirectInput exports. The .asi build is loaded by the ASI loader and
// exports nothing.
void dinput8_proxy_load();
#endif

BOOL WINAPI DllMain(HINSTANCE inst, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        g_self = inst;
        DisableThreadLibraryCalls(inst);

#ifdef T8T_DINPUT8_PROXY
        // Before anything can call an export, and before the scan thread runs.
        dinput8_proxy_load();
#endif

        // The scan runs off the loader lock: a ~100 MB .text sweep would stall
        // startup, and the takeover code is minutes of menu navigation away.
        if (const HANDLE t = CreateThread(nullptr, 0, worker, nullptr, 0, nullptr))
            CloseHandle(t);
    }
    return TRUE; // a miss must never stop the game from loading
}
