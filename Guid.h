#pragma once
#include <cstdint>

#include <nmmintrin.h>

class Guid
{
public:
    Guid()
    {
        //memset(_uuid.data, 0, 16);
    }

    Guid(const char input[16])
    {
        //memmove(_uuid.data, input, 16);
    }

    static __m128i loadSimdCharArray(const char& chr)
    {
        __declspec(align(16)) const char y[17] = { chr, chr, chr, chr, chr, chr, chr, chr, chr, chr, chr, chr, chr, chr, chr, chr };
        return _mm_load_si128(reinterpret_cast<const __m128i*>(&y));
    }

    static __m128i loadAllowedCharRange()
    {
        __declspec(align(16)) const char _charRanges[17] = { '0', '9', 'A', 'Z', 'a', 'z', 0, -1, 0, -1, 0, -1, 0, -1, 0, -1 };
        return _mm_load_si128(reinterpret_cast<const __m128i*>(&_charRanges));
    }

    int Parse(const char* ptr)
    {
        const __m128i str = _mm_loadu_si128(reinterpret_cast<const __m128i *>(ptr));
        const __m128i str2 = _mm_loadu_si128(reinterpret_cast<const __m128i *>(ptr + 19));

        const __m128i allowedCharRange = loadAllowedCharRange();

        auto cmp = _mm_cmpistrm(allowedCharRange, str, _SIDD_UBYTE_OPS | _SIDD_CMP_RANGES | _SIDD_NEGATIVE_POLARITY);
        auto cmp2 = _mm_cmpistrm(allowedCharRange, str2, _SIDD_UBYTE_OPS | _SIDD_CMP_RANGES | _SIDD_NEGATIVE_POLARITY);
        auto cmpEq = _mm_xor_si128(cmp, _mm_set_epi64x(0LL, 8448LL));
        auto cmpEq2 = _mm_xor_si128(cmp2, _mm_set_epi64x(0LL, 16LL));

        // Checks if all chars (except all the expected '-') are between: 0-9, A-Z and a-z)
        if (!_mm_testz_si128(cmpEq, cmpEq) ||
            !_mm_testz_si128(cmpEq2, cmpEq2))
            return -1;

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

        mask1 = _mm_insert_epi8(mask1, ptr[16], 7);
        mask2 = _mm_insert_epi8(mask2, ptr[17], 7);
        mask3 = _mm_insert_epi8(mask3, ptr[34], 15);
        mask4 = _mm_insert_epi8(mask4, ptr[35], 15);

        mask1 = _mm_blend_epi16(mask1, mask3, 0xF0);
        mask2 = _mm_blend_epi16(mask2, mask4, 0xF0);

        const __m128i nine = loadSimdCharArray('9');
        const __m128i aboveNineMask1 = _mm_cmpgt_epi8(mask1, nine);
        const __m128i aboveNineMask2 = _mm_cmpgt_epi8(mask2, nine);

        const __m128i and1 = _mm_and_si128(mask1, aboveNineMask1);
        const __m128i and2 = _mm_and_si128(mask2, aboveNineMask2);

        const __m128i toLowerCase = loadSimdCharArray(0x20);
        const __m128i or1 = _mm_or_si128(and1, toLowerCase);
        const __m128i or2 = _mm_or_si128(and2, toLowerCase);

        const __m128i toHex = loadSimdCharArray('a' - 10 - '0');
        const __m128i fixedUppercase1 = _mm_sub_epi8(or1, toHex);
        const __m128i fixedUppercase2 = _mm_sub_epi8(or2, toHex);

        mask1 = _mm_blendv_epi8(mask1, fixedUppercase1, aboveNineMask1);
        mask2 = _mm_blendv_epi8(mask2, fixedUppercase2, aboveNineMask2);

        // 12%
        const __m128i zero = loadSimdCharArray('0');
        __m128i hi = _mm_sub_epi8(mask1, zero);
        hi = _mm_slli_epi16(hi, 4);
        const __m128i lo = _mm_sub_epi8(mask2, zero);

        // 9%
        _mm_store_si128(reinterpret_cast<__m128i *>(&_uuid.data[0]), _mm_xor_si128(hi, lo));
        // 10%
        return 0;
    }

private:
    union alignas(16) uuid
    {
        unsigned char data[16];
        uint64_t ulongs[2];
        uint32_t uints[4];
    };

    alignas(128) uuid _uuid;
};
