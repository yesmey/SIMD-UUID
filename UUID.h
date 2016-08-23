#pragma once
#include <cstdint>

#include <nmmintrin.h>
#include <type_traits>

#if defined(_MSC_VER)
#define ALIGNED_(size) __declspec(align(size))
#define NO_INLINE __declspec(noinline)
#else
#define ALIGNED_(size) __attribute__((__aligned__(size)))
#define NO_INLINE __attribute__ ((noinline))
#endif

enum BracketType
{
    BRACKET_NONE = 0,
    BRACKET_PARENTHESIS,
    BRACKET_BRACES
};

enum HexCaseType
{
    HEX_CASE_NONE = 0,
    HEX_CASE_LOWER,
    HEX_CASE_UPPER
};

class UUID final
{
public:
    UUID() = default;
    ~UUID() = default;

    static __m128i loadSimdCharArray(const char& chr)
    {
        ALIGNED_(16) const char y[17] = { chr, chr, chr, chr, chr, chr, chr, chr, chr, chr, chr, chr, chr, chr, chr, chr };
        return _mm_load_si128(reinterpret_cast<const __m128i*>(&y));
    }

    static __m128i loadAllowedCharRange()
    {
        ALIGNED_(16) const char _charRanges[17] = { '0', '9', 'A', 'Z', 'a', 'z', 0, -1, 0, -1, 0, -1, 0, -1, 0, -1 };
        return _mm_load_si128(reinterpret_cast<const __m128i*>(&_charRanges));
    }

    template<BracketType bracket = BRACKET_NONE, HexCaseType hexCase = HEX_CASE_NONE>
    int Parse(const char* ptr)
    {
        if (bracket == BRACKET_BRACES)
        {
            // Expect the string string to start and end with bracers (no trailing whitespace allowed)
            if (ptr[0] == '{' && ptr[37] == '}' && ptr[38] == '\0')
                return innerParse<hexCase>(ptr + 1);
            return 1;
        }
        if (bracket == BRACKET_PARENTHESIS)
        {
            // Same as above with parenthesis
            if (ptr[0] == '(' && ptr[37] == ')' && ptr[38] == '\0')
                return innerParse<hexCase>(ptr + 1);
            return 1;
        }
        return innerParse<hexCase>(ptr);
    }

private:

    template<HexCaseType hexCase = HEX_CASE_NONE>
    int innerParse(const char* ptr)
    {
        const __m128i str = _mm_loadu_si128(reinterpret_cast<const __m128i *>(ptr));
        const __m128i str2 = _mm_loadu_si128(reinterpret_cast<const __m128i *>(ptr + 19));

        const __m128i MASK_1 = _mm_setr_epi8(
            0, 2, 4, 6,
            9, 11, 14, -1,
            -1, -1, -1, -1,
            -1, -1, -1, -1);

        const __m128i MASK_2 = _mm_setr_epi8(
            1, 3, 5, 7,
            10, 12, 15, -1,
            -1, -1, -1, -1,
            -1, -1, -1, -1);

        const __m128i MASK_3 = _mm_setr_epi8(
            -1, -1, -1, -1,
            -1, -1, -1, -1,
            0, 2, 5, 7,
            9, 11, 13, -1);

        const __m128i MASK_4 = _mm_setr_epi8(
            -1, -1, -1, -1,
            -1, -1, -1, -1,
            1, 3, 6, 8,
            10, 12, 14, -1);

        __m128i mask1 = _mm_shuffle_epi8(str, MASK_1);
        __m128i mask2 = _mm_shuffle_epi8(str, MASK_2);

        __m128i mask3 = _mm_shuffle_epi8(str2, MASK_3);
        __m128i mask4 = _mm_shuffle_epi8(str2, MASK_4);

        // Since we had hypens between the character we have 36 characters which does not fit in two 16 char loads
        // therefor we must manually insert them here
        mask1 = _mm_insert_epi8(mask1, ptr[16], 7);
        mask2 = _mm_insert_epi8(mask2, ptr[17], 7);
        mask3 = _mm_insert_epi8(mask3, ptr[34], 15);
        mask4 = _mm_insert_epi8(mask4, ptr[35], 15);

        // Merge [aaaaaaaa|aaaaaaaa|00000000|00000000] | [00000000|00000000|bbbbbbbb|bbbbbbbb]
        mask1 = _mm_or_si128(mask1, mask3);
        mask2 = _mm_or_si128(mask2, mask4);

        // Check if all characters are between 0-9, A-Z or a-z
        const __m128i allowedCharRange = loadAllowedCharRange();
        const int cmp = _mm_cmpistri(allowedCharRange, mask1, _SIDD_UBYTE_OPS | _SIDD_CMP_RANGES | _SIDD_NEGATIVE_POLARITY);
        const int cmp2 = _mm_cmpistri(allowedCharRange, mask2, _SIDD_UBYTE_OPS | _SIDD_CMP_RANGES | _SIDD_NEGATIVE_POLARITY);

        if (cmp == 16 && cmp2 == 16)
        {
            const __m128i nine = loadSimdCharArray('9');
            const __m128i aboveNineMask1 = _mm_cmpgt_epi8(mask1, nine);
            const __m128i aboveNineMask2 = _mm_cmpgt_epi8(mask2, nine);

            __m128i letterMask1 = _mm_and_si128(mask1, aboveNineMask1);
            __m128i letterMask2 = _mm_and_si128(mask2, aboveNineMask2);

            // When we only have lowercase we don't need to look for upper case
            if (hexCase != HEX_CASE_LOWER)
            {
                // Convert upper case (A-Z) to lower case (a-z)
                const __m128i toLowerCase = loadSimdCharArray(0x20);
                letterMask1 = _mm_or_si128(letterMask1, toLowerCase);
                letterMask2 = _mm_or_si128(letterMask2, toLowerCase);
            }

            const __m128i toHex = loadSimdCharArray('a' - 10 - '0');
            const __m128i fixedUppercase1 = _mm_sub_epi8(letterMask1, toHex);
            const __m128i fixedUppercase2 = _mm_sub_epi8(letterMask2, toHex);

            mask1 = _mm_blendv_epi8(mask1, fixedUppercase1, aboveNineMask1);
            mask2 = _mm_blendv_epi8(mask2, fixedUppercase2, aboveNineMask2);

            const __m128i zero = loadSimdCharArray('0');
            __m128i hi = _mm_sub_epi8(mask1, zero);
            hi = _mm_slli_epi16(hi, 4);
            const __m128i lo = _mm_sub_epi8(mask2, zero);

            _mm_store_si128(reinterpret_cast<__m128i *>(&_uuid.data[0]), _mm_xor_si128(hi, lo));
            return 0;
        }

        return 1;
    }
    public:
    union alignas(16) uuid
    {
        unsigned char data[16];
        uint64_t ulongs[2];
        uint32_t uints[4];
    };

    alignas(128) uuid _uuid;
};
