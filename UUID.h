#pragma once

#include <cstdint>
#include <string>
#include <array>
#include <nmmintrin.h>

#if defined(_MSC_VER)
#define ALIGNED_(size) __declspec(align(size))
#define NO_INLINE __declspec(noinline)
#else
#define ALIGNED_(size) __attribute__((__aligned__(size)))
#define NO_INLINE __attribute__ ((noinline))
#endif

namespace meyer {
    class UUID final
    {
    public:
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

        enum ValidationType
        {
            VALIDATE_NONE = 0,
            VALIDATE_ALL
        };

        UUID() = default;
        ~UUID() = default;

        UUID(std::string const& str)
        {
            Parse(str);
        }

        UUID(UUID const& val)
        {
            _mm_store_si128(reinterpret_cast<__m128i *>(_data), _mm_load_si128(reinterpret_cast<const __m128i *>(&val._data[0])));
        }

        template<BracketType bracket = BRACKET_NONE, HexCaseType hexCase = HEX_CASE_NONE, ValidationType validation = VALIDATE_ALL>
        int Parse(std::string const& str)
        {
            const char* ptr = str.c_str();
            if (bracket == BRACKET_BRACES)
            {
                // Expect the string string to start and end with bracers (no trailing whitespace allowed)
                if (ptr[0] == '{' && ptr[37] == '}' && str.length() == 38)
                    return innerParse<hexCase, validation>(ptr + 1);
            }
            if (bracket == BRACKET_PARENTHESIS)
            {
                // Same as above with parenthesis
                if (ptr[0] == '(' && ptr[37] == ')' && str.length() == 38)
                    return innerParse<hexCase, validation>(ptr + 1);
            }
            else
            {
                if (str.length() == 36)
                    return innerParse<hexCase, validation>(ptr);
            }
            return 1;
        }

        unsigned char const* Data() const
        {
            return &_data[0];
        }

        bool operator ==(UUID const& other) const noexcept
        {
            __m128i mm_left = _mm_load_si128(reinterpret_cast<const __m128i*>(_data));
            __m128i mm_right = _mm_load_si128(reinterpret_cast<const __m128i*>(other._data));
            // Very simple comparison. xor left and right and check if any of them are non-zero.
            return _mm_test_all_zeros(_mm_xor_si128(mm_left, mm_right), _mm_setzero_si128()) != 0;
        }

        bool operator !=(UUID const& other) const noexcept
        {
            return !(*this == other);
        }

        std::array<char, 36> to_string() const
        {
            const __m128i a = _mm_set1_epi8(0x0F);
            __m128i lower = _mm_load_si128(reinterpret_cast<const __m128i*>(_data));
            __m128i upper = _mm_and_si128(_mm_set1_epi8(0xFF >> 4), _mm_srli_epi32(lower, 4));

            lower = _mm_and_si128(lower, a);
            upper = _mm_and_si128(upper, a);

            const __m128i pastNine = loadSimdCharArray(10);
            const __m128i lowerMask = _mm_cmplt_epi8(lower, pastNine);
            const __m128i upperMask = _mm_cmplt_epi8(upper, pastNine);

            __m128i letterMask1 = _mm_and_si128(lower, lowerMask);
            __m128i letterMask2 = _mm_and_si128(upper, upperMask);

            __m128i letterMask3 = _mm_or_si128(lower, lowerMask);
            __m128i letterMask4 = _mm_or_si128(upper, upperMask);

            const __m128i first = _mm_set1_epi8('0');
            const __m128i second = _mm_set1_epi8('a' - 10);

            letterMask1 = _mm_add_epi8(letterMask1, first);
            letterMask2 = _mm_add_epi8(letterMask2, first);

            letterMask3 = _mm_add_epi8(letterMask3, second);
            letterMask4 = _mm_add_epi8(letterMask4, second);

            lower = _mm_blendv_epi8(letterMask3, letterMask1, lowerMask);
            upper = _mm_blendv_epi8(letterMask4, letterMask2, upperMask);

            const __m128i MASK_1 = _mm_setr_epi8(
                -1, 0, -1, 1,
                -1, 2, -1, 3,
                -1, -1, 4, -1,
                5, -1, -1, 6);

            const __m128i MASK_2 = _mm_setr_epi8(
                0, -1, 1, -1,
                2, -1, 3, -1,
                -1, 4, -1, 5,
                -1, -1, 6, -1);

            const __m128i MASK_3 = _mm_setr_epi8(
                -1, 7, -1, -1,
                8, -1, 9, -1,
                -1, 10, -1, 11,
                -1, 12, -1, 13);

            const __m128i MASK_4 = _mm_setr_epi8(
                7, -1, -1, 8,
                -1, 9, -1, -1,
                10, -1, 11, -1,
                12, -1, 13, -1);

            const __m128i mask1 = _mm_shuffle_epi8(lower, MASK_1);
            const __m128i mask2 = _mm_shuffle_epi8(upper, MASK_2);
            const __m128i mask3 = _mm_shuffle_epi8(lower, MASK_3);
            const __m128i mask4 = _mm_shuffle_epi8(upper, MASK_4);

            const __m128i bind = _mm_set_epi8(0, 0, '-', 0, 0, 0, 0, '-', 0, 0, 0, 0, 0, 0, 0, 0);
            const __m128i bind2 = _mm_set_epi8(0, 0, 0, 0, 0, 0, 0, 0, '-', 0, 0, 0, 0, '-', 0, 0);

            const __m128i upperSorted = _mm_or_si128(_mm_or_si128(mask1, mask2), bind);
            const __m128i lowerSorted = _mm_or_si128(_mm_or_si128(mask3, mask4), bind2);

            ALIGNED_(16) std::array<char, 36> result;
            _mm_store_si128(reinterpret_cast<__m128i *>(result.data()), upperSorted);
            _mm_store_si128(reinterpret_cast<__m128i *>(result.data() + 16), lowerSorted);

            // Did not fit the last four chars. Extract and append them.
            const int v1 = _mm_extract_epi16(upper, 7);
            const int v2 = _mm_extract_epi16(lower, 7);
            result[32] = v1 & 0xff;
            result[33] = v2 & 0xff;
            result[34] = (v1 >> 8) & 0xff;
            result[35] = (v2 >> 8) & 0xff;
            return result;
        }

    private:
        template<HexCaseType hexCase = HEX_CASE_NONE, ValidationType validation = VALIDATE_ALL>
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
            if (validation == VALIDATE_ALL)
            {
                const __m128i allowedCharRange = loadAllowedCharRange<hexCase>();
                const int cmp = _mm_cmpistri(allowedCharRange, mask1, _SIDD_UBYTE_OPS | _SIDD_CMP_RANGES | _SIDD_NEGATIVE_POLARITY);
                const int cmp2 = _mm_cmpistri(allowedCharRange, mask2, _SIDD_UBYTE_OPS | _SIDD_CMP_RANGES | _SIDD_NEGATIVE_POLARITY);
                if (cmp != 16 || cmp2 != 16)
                    return 1;
            }

            const __m128i nine = loadSimdCharArray('9');
            const __m128i aboveNineMask1 = _mm_cmpgt_epi8(mask1, nine);
            const __m128i aboveNineMask2 = _mm_cmpgt_epi8(mask2, nine);

            __m128i letterMask1 = _mm_and_si128(mask1, aboveNineMask1);
            __m128i letterMask2 = _mm_and_si128(mask2, aboveNineMask2);

            // Convert upper case when we are mixing upper with lower case
            if (hexCase == HEX_CASE_NONE)
            {
                // Convert upper case (A-Z) to lower case (a-z)
                const __m128i toLowerCase = loadSimdCharArray(0x20);
                letterMask1 = _mm_or_si128(letterMask1, toLowerCase);
                letterMask2 = _mm_or_si128(letterMask2, toLowerCase);
            }

            const __m128i toHex = (hexCase == HEX_CASE_UPPER) ? loadSimdCharArray('A' - 10 - '0') : loadSimdCharArray('a' - 10 - '0');
            const __m128i fixedUppercase1 = _mm_sub_epi8(letterMask1, toHex);
            const __m128i fixedUppercase2 = _mm_sub_epi8(letterMask2, toHex);

            //mask1 = _mm_or_si128(_mm_and_si128(aboveNineMask1, fixedUppercase1), _mm_andnot_si128(aboveNineMask1, mask1));
            //mask2 = _mm_or_si128(_mm_and_si128(aboveNineMask2, fixedUppercase2), _mm_andnot_si128(aboveNineMask2, mask2));
            mask1 = _mm_blendv_epi8(mask1, fixedUppercase1, aboveNineMask1);
            mask2 = _mm_blendv_epi8(mask2, fixedUppercase2, aboveNineMask2);

            const __m128i zero = loadSimdCharArray('0');
            const __m128i lo = _mm_sub_epi8(mask2, zero);
            const __m128i hi = _mm_slli_epi16(_mm_sub_epi8(mask1, zero), 4);

            _mm_store_si128(reinterpret_cast<__m128i *>(_data), _mm_xor_si128(hi, lo));
            return 0;
        }

        /*
        * Helper method for loading an aligned 16 byte repeated sse val
        */

        static __m128i loadSimdCharArray(const char& chr)
        {
            ALIGNED_(16) const char y[17] = { chr, chr, chr, chr, chr, chr, chr, chr, chr, chr, chr, chr, chr, chr, chr, chr };
            return _mm_load_si128(reinterpret_cast<const __m128i*>(y));
        }

        /*
        * Helper method for loading the allowed char range depending on HexCaseType (replace with constexpr if whenever avaliable)
        */

        template<HexCaseType hexCase>
        auto static loadAllowedCharRange() -> typename std::enable_if<hexCase == HEX_CASE_UPPER, __m128i>::type
        {
            ALIGNED_(16) const char _charRanges[17] = { '0', '9', 'A', 'Z', 0, -1, 0, -1, 0, -1, 0, -1, 0, -1, 0, -1 };
            return _mm_load_si128(reinterpret_cast<const __m128i*>(_charRanges));
        }
        template<HexCaseType hexCase>
        auto static loadAllowedCharRange() -> typename std::enable_if<hexCase == HEX_CASE_LOWER, __m128i>::type
        {
            ALIGNED_(16) const char _charRanges[17] = { '0', '9', 'a', 'z', 0, -1, 0, -1, 0, -1, 0, -1, 0, -1, 0, -1 };
            return _mm_load_si128(reinterpret_cast<const __m128i*>(_charRanges));
        }
        template<HexCaseType hexCase>
        auto static loadAllowedCharRange() -> typename std::enable_if<hexCase == HEX_CASE_NONE, __m128i>::type
        {
            ALIGNED_(16) const char _charRanges[17] = { '0', '9', 'A', 'Z', 'a', 'z', 0, -1, 0, -1, 0, -1, 0, -1, 0, -1 };
            return _mm_load_si128(reinterpret_cast<const __m128i*>(_charRanges));
        }

        ALIGNED_(16) unsigned char _data[16];
    };
}
