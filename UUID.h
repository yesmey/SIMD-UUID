#pragma once
#include <cstdint>
#include <string>
#include <nmmintrin.h>

#if defined(_MSC_VER)
#define ALIGNED_(size) __declspec(align(size))
#define NO_INLINE __declspec(noinline)
#include <Windows.h>
#include <Wincrypt.h>
#else
#define ALIGNED_(size) __attribute__((__aligned__(size)))
#define NO_INLINE __attribute__ ((noinline))
#endif

namespace meyr {
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

        UUID(std::string const& str)
        {
            Parse(str);
        }

        UUID(UUID const& val)
        {
            _mm_store_si128(reinterpret_cast<__m128i *>(&_uuid.data[0]), _mm_load_si128(reinterpret_cast<const __m128i *>(&val._uuid.data[0])));
        }

        ~UUID() = default;

        template<BracketType bracket = BRACKET_NONE, HexCaseType hexCase = HEX_CASE_NONE, ValidationType validation = VALIDATE_ALL>
        int Parse(std::string const& str)
        {
            auto ptr = str.c_str();
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
            return &_uuid.data[0];
        }

        bool operator== (UUID const& other) const noexcept
        {
            __m128i mm_left = _mm_load_si128(reinterpret_cast<const __m128i*>(_uuid.data));
            __m128i mm_right = _mm_load_si128(reinterpret_cast<const __m128i*>(other._uuid.data));
            __m128i test = _mm_cmpeq_epi32(mm_left, mm_right);
            return _mm_testc_si128(test, _mm_cmpeq_epi32((_mm_cmpeq_epi32(mm_left, mm_right)),(_mm_cmpeq_epi32(mm_left, mm_right)))) != 0;
        }
        bool operator!=(UUID const& other) const noexcept
        {
            return !(*this == other);
        }

        bool operator< (UUID const& other) const noexcept
        {
            __m128i mm_left = _mm_load_si128(reinterpret_cast<const __m128i*>(_uuid.data));
            __m128i mm_right = _mm_load_si128(reinterpret_cast<const __m128i*>(other._uuid.data));

            const __m128i mm_signs_mask = _mm_xor_si128(mm_left, mm_right);
            __m128i mm_cmp = _mm_cmpgt_epi8(mm_right, mm_left);
            __m128i mm_rcmp = _mm_cmpgt_epi8(mm_left, mm_right);
            mm_cmp = _mm_xor_si128(mm_signs_mask, mm_cmp);
            mm_rcmp = _mm_xor_si128(mm_signs_mask, mm_rcmp);

            uint32_t cmp = static_cast<uint32_t>(_mm_movemask_epi8(mm_cmp));
            uint32_t rcmp = static_cast<uint32_t>(_mm_movemask_epi8(mm_rcmp));
            cmp = (cmp - 1u) ^ cmp;
            rcmp = (rcmp - 1u) ^ rcmp;
            return cmp < rcmp;
        }
        bool operator>(UUID const& rhs) const noexcept
        {
            return *this > rhs;
        }
        bool operator<=(UUID const& rhs) const noexcept
        {
            return !(*this > rhs);
        }
        bool operator>=(UUID const& rhs) const noexcept
        {
            return !(*this < rhs);
        }

        /*std::string to_string() const
        {
            std::string result;
            result.reserve(36);

            for (int i = 0; i < 16; ++i) {
                const size_t hi = (_uuid.data[i] >> 4) & 0x0F;
                if (hi <= 9) {
                    result.push_back(static_cast<char>('0' + hi));
                }
                else {
                    result.push_back(static_cast<char>('a' + (hi - 10)));
                }

                const size_t lo = _uuid.data[i] & 0x0F;
                if (lo <= 9) {
                    result.push_back(static_cast<char>('0' + lo));
                }
                else {
                    result.push_back(static_cast<char>('a' + (lo - 10)));
                }

                if (i == 3 || i == 5 || i == 7 || i == 9) {
                    result.push_back('-');
                }
            }
            return result;
        }*/

        std::string to_string() const
        {
            const auto a = _mm_set1_epi8(0x0F);
            __m128i lower = _mm_load_si128(reinterpret_cast<const __m128i*>(_uuid.data));
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

            const auto first = _mm_set1_epi8('0');
            const auto second = _mm_set1_epi8('a' - 10);

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
                -1, 7, -1, -1, 8,
                -1, 9, -1, -1,
                10, -1, 11, -1, 12,
                -1, 13);

            const __m128i MASK_4 = _mm_setr_epi8(
                7, -1, -1, 8,
                -1, 9, -1, -1, 10, 
                -1, 11, -1, 12,
                -1, 13, -1);

            const __m128i bind = _mm_set_epi8(0, 0, '-', 0, 0, 0, 0, '-', 0, 0, 0, 0, 0, 0, 0, 0);
            const __m128i mask1 = _mm_shuffle_epi8(lower, MASK_1);
            const __m128i mask2 = _mm_shuffle_epi8(upper, MASK_2);

            __m128i upperSorted = _mm_or_si128(mask1, mask2);
            upperSorted = _mm_or_si128(upperSorted, bind);

            const __m128i mask3 = _mm_shuffle_epi8(lower, MASK_3);
            const __m128i mask4 = _mm_shuffle_epi8(upper, MASK_4);

            const __m128i bind2 = _mm_set_epi8(0, 0, 0, 0, 0, 0, 0, 0, '-', 0, 0, 0, 0, '-', 0, 0);
            __m128i lowerSorted = _mm_or_si128(mask3, mask4);
            lowerSorted = _mm_or_si128(lowerSorted, bind2);

            ALIGNED_(16) char result[36];
            _mm_store_si128(reinterpret_cast<__m128i *>(result), upperSorted);
            _mm_store_si128(reinterpret_cast<__m128i *>(result + 16), lowerSorted);

            const uint16_t v1 = _mm_extract_epi16(upper, 4);
            const uint16_t v2 = _mm_extract_epi16(lower, 4);
            // meh...
            result[32] = v1 & 0xff;
            result[33] = v2 & 0xff;
            result[34] = (v1 >> 8) & 0xff;
            result[35] = (v2 >> 8) & 0xff;
            return std::string(result, 36);
        }

    private:

        template<HexCaseType hexCase = HEX_CASE_NONE, ValidationType validation = VALIDATE_ALL>
        int innerParse(const char* ptr)
        {
            const __m128i str = _mm_lddqu_si128(reinterpret_cast<const __m128i *>(ptr));
            const __m128i str2 = _mm_lddqu_si128(reinterpret_cast<const __m128i *>(ptr + 19));

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
            __m128i hi = _mm_sub_epi8(mask1, zero);
            hi = _mm_slli_epi16(hi, 4);
            const __m128i lo = _mm_sub_epi8(mask2, zero);

            _mm_store_si128(reinterpret_cast<__m128i *>(_uuid.data), _mm_xor_si128(hi, lo));
            return 0;
        }

        union alignas(16) uuid
        {
            unsigned char data[16];
            uint32_t uints[4];
            uint64_t ulongs[2];
        };

        alignas(128) uuid _uuid;

        /*
        * Helper method for loading an aligned 16 byte repeated sse val
        */

        static __m128i loadSimdCharArray(const char& chr)
        {
            ALIGNED_(16) const char y[17] = { chr, chr, chr, chr, chr, chr, chr, chr, chr, chr, chr, chr, chr, chr, chr, chr };
            return _mm_load_si128(reinterpret_cast<const __m128i*>(&y));
        }

        /*
        * Helper method for loading the allowed char range depending on HexCaseType (replace with constexpr if whenever avaliable)
        */

        template<HexCaseType hexCase>
        auto static loadAllowedCharRange() -> typename std::enable_if<hexCase == HEX_CASE_UPPER, __m128i>::type
        {
            ALIGNED_(16) const char _charRanges[17] = { '0', '9', 'A', 'Z', 0, -1, 0, -1, 0, -1, 0, -1, 0, -1, 0, -1 };
            return _mm_load_si128(reinterpret_cast<const __m128i*>(&_charRanges));
        }
        template<HexCaseType hexCase>
        auto static loadAllowedCharRange() -> typename std::enable_if<hexCase == HEX_CASE_LOWER, __m128i>::type
        {
            ALIGNED_(16) const char _charRanges[17] = { '0', '9', 'a', 'z', 0, -1, 0, -1, 0, -1, 0, -1, 0, -1, 0, -1 };
            return _mm_load_si128(reinterpret_cast<const __m128i*>(&_charRanges));
        }
        template<HexCaseType hexCase>
        auto static loadAllowedCharRange() -> typename std::enable_if<hexCase == HEX_CASE_NONE, __m128i>::type
        {
            ALIGNED_(16) const char _charRanges[17] = { '0', '9', 'A', 'Z', 'a', 'z', 0, -1, 0, -1, 0, -1, 0, -1, 0, -1 };
            return _mm_load_si128(reinterpret_cast<const __m128i*>(&_charRanges));
        }

        friend class UUIDGenerator;
    };

    class UUIDGenerator
    {
    public:
        UUIDGenerator()
        {
            // if windows...
            HCRYPTPROV random;
            if (CryptAcquireContext(
                &random,
                nullptr,
                nullptr,
                PROV_RSA_FULL,
                0))
            {
                ALIGNED_(16) unsigned char state[32];
                CryptGenRandom(random, sizeof(state), state);

                seed1 = _mm_load_si128(reinterpret_cast<const __m128i*>(&state[0]));
                seed2 = _mm_load_si128(reinterpret_cast<const __m128i*>(&state[16]));

                CryptReleaseContext(random, 0);
            }
        }

        UUID operator()()
        {
            UUID newUuid;
            const __m128i mask = loadSimdIntArray(0x0000FFFF);
            const __m128i m1 = loadSimdIntArray(0x4650);
            const __m128i m2 = loadSimdIntArray(0x78B7);
            
            __m128i amask = _mm_and_si128(seed1, mask);
            __m128i ashift = _mm_srli_epi32(seed1, 0x10);
            __m128i amul = _mm_mullo_epi32(amask, m1);
            __m128i anew = _mm_add_epi32(amul, ashift);
            seed1 = anew;

            __m128i bmask = _mm_and_si128(seed2, mask);
            __m128i bshift = _mm_srli_epi32(seed2, 0x10);
            __m128i bmul = _mm_mullo_epi32(bmask, m2);
            __m128i bnew = _mm_add_epi32(bmul, bshift);
            seed2 = bnew;

            __m128i bmasknew = _mm_and_si128(bnew, mask);
            __m128i ashiftnew = _mm_slli_epi32(anew, 0x10);

            __m128i result = _mm_or_si128(ashiftnew, bmasknew);
            _mm_store_si128(reinterpret_cast<__m128i *>(newUuid._uuid.data), result);

            *(&newUuid._uuid.data[6]) &= 0x4F;
            *(&newUuid._uuid.data[6]) |= 0x40;

            *(&newUuid._uuid.data[8]) &= 0xBF;
            *(&newUuid._uuid.data[8]) |= 0x80;
            return newUuid;
        }

    private:
        __m128i seed1, seed2;

        static __m128i loadSimdIntArray(const int& val)
        {
            ALIGNED_(16) const int y[4] = { val, val, val, val };
            return _mm_load_si128(reinterpret_cast<const __m128i*>(&y));
        }
    };
}
