// Self-check for the pattern decoder and the scanner.
//
// A decoder bug produces a wrong byte string, which scans as NOT FOUND — the
// same line a real game patch produces. This is the check that tells those two
// apart, so run it whenever takeover.cpp changes:
//
//     cmake --build build --config Release --target selftest
//     ./build/Release/selftest.exe
//
// Includes the .cpp so it can reach the anonymous-namespace helpers.

#include "takeover.cpp"

// Release defines NDEBUG, which turns assert() into (void)0 and deletes the
// calls inside it — the whole check would silently pass. Force it on.
#undef NDEBUG
#include <cassert>
#include <cstdio>
#include <vector>

int main()
{
    uint8_t bytes[64];
    bool    mask[64];
    size_t  len = 0;

    // Plain hex.
    assert(decode("41 B8 58 02", bytes, mask, sizeof(bytes), &len));
    assert(len == 4);
    assert(bytes[0] == 0x41 && bytes[1] == 0xB8 && bytes[2] == 0x58 && bytes[3] == 0x02);
    assert(mask[0] && mask[1] && mask[2] && mask[3]);

    // Wildcards, both spellings, and lower case.
    assert(decode("41 ?? b8 ?", bytes, mask, sizeof(bytes), &len));
    assert(len == 4);
    assert(mask[0] && !mask[1] && mask[2] && !mask[3]);
    assert(bytes[2] == 0xB8);

    // Garbage is rejected, not silently truncated.
    assert(!decode("4G", bytes, mask, sizeof(bytes), &len));
    assert(!decode("", bytes, mask, sizeof(bytes), &len));

    // Every real site: the pattern decodes, and imm_off lands on the 600 the
    // patcher expects. This is what catches a hand-miscounted offset.
    for (const Site& site : kSites)
    {
        assert(decode(site.hex, bytes, mask, sizeof(bytes), &len));
        assert(site.imm_off + sizeof(uint32_t) <= len);

        uint32_t imm = 0;
        memcpy(&imm, bytes + site.imm_off, sizeof(imm));
        assert(imm == kVanilla);

        // One copy in a haystack -> exactly one hit, at the right address.
        std::vector<uint8_t> hay(4096, 0x90);
        memcpy(hay.data() + 1000, bytes, len);

        const uint8_t* hit  = nullptr;
        size_t         hits = scan(hay.data(), hay.size(), bytes, mask, len, &hit);
        assert(hits == 1);
        assert(hit == hay.data() + 1000);

        // A second copy must make it ambiguous, never "first wins".
        memcpy(hay.data() + 2000, bytes, len);
        hits = scan(hay.data(), hay.size(), bytes, mask, len, &hit);
        assert(hits > 1);

        // Absent -> no hit.
        std::vector<uint8_t> empty(4096, 0x90);
        hits = scan(empty.data(), empty.size(), bytes, mask, len, &hit);
        assert(hits == 0);

        // Buffer smaller than the pattern must not read past the end.
        hits = scan(empty.data(), len - 1, bytes, mask, len, &hit);
        assert(hits == 0);

        printf("ok: %s (%zu bytes, imm at +%zu)\n", site.name, len, site.imm_off);
    }

    printf("all checks passed\n");
    return 0;
}
