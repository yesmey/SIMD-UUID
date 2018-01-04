#pragma once

#include <cstdint>
#include <string_view>
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

    enum UUIDFlags : uint8_t
    {
        NONE                = 0x00,

        CASE_LOWER          = 0x01,
        CASE_UPPER          = 0x02,

        BRACKET_PARENTHESIS = 0x04,
        BRACKET_BRACES      = 0x08,

        VALIDATE_ALL        = 0x10,
    };

    constexpr enum UUIDFlags operator |(const enum UUIDFlags selfValue, const enum UUIDFlags inValue)
    {
        return (enum UUIDFlags)(uint8_t(selfValue) | uint8_t(inValue));
    }

    constexpr enum UUIDFlags operator &(const enum UUIDFlags selfValue, const enum UUIDFlags inValue)
    {
        return (enum UUIDFlags)(uint8_t(selfValue) & uint8_t(inValue));
    }

    template<UUIDFlags Tflag = NONE, typename = typename std::enable_if<std::is_enum<UUIDFlags>::value, void>::type>
    class UUID final
    {
        static_assert((Tflag & (CASE_LOWER | CASE_UPPER)) != (CASE_LOWER | CASE_UPPER),
            "Cannot combine flags CASE_LOWER with CASE_UPPER");
        static_assert((Tflag & (BRACKET_PARENTHESIS | BRACKET_BRACES)) != (BRACKET_PARENTHESIS | BRACKET_BRACES),
            "Cannot combine flags BRACKET_PARENTHESIS with BRACKET_BRACES");

    public:
        UUID() = default;
        ~UUID() = default;

        UUID(std::string_view str)
        {
            parse(str);
        }

        UUID(UUID const& val)
        {
            _mm_store_si128(reinterpret_cast<__m128i *>(_data), _mm_load_si128(reinterpret_cast<const __m128i *>(&val._data[0])));
        }

        int parse(std::string_view str)
        {
            const char* ptr = str.data();
            if ((Tflag & BRACKET_BRACES) != 0)
            {
                // Expect the string string to start and end with bracers (no trailing whitespace allowed)
                if (ptr[0] == '{' && ptr[37] == '}' && str.length() == 38)
                    return innerParse(ptr + 1);
            }
            else if ((Tflag & BRACKET_PARENTHESIS) != 0)
            {
                // Same as above with parenthesis
                if (ptr[0] == '(' && ptr[37] == ')' && str.length() == 38)
                    return innerParse(ptr + 1);
            }
            else
            {
                if (str.length() == 38) {
                    if ((ptr[0] == '(' && ptr[37] == ')') || (ptr[0] == '{' && ptr[37] == '}'))
                        return innerParse(ptr + 1);
                }
                else if (str.length() == 36)
                    return innerParse(ptr);
            }
            return 0;
        }

        unsigned char const* get_data() const
        {
            return &_data[0];
        }

        // null terminated guid in a char array
        std::array<char, 37> to_string() const
        {
            const __m128i a = _mm_set1_epi8(0x0F);
            __m128i lower = _mm_load_si128(reinterpret_cast<const __m128i*>(_data));
            __m128i upper = _mm_and_si128(_mm_set1_epi8(0xFF >> 4), _mm_srli_epi32(lower, 4));

            lower = _mm_and_si128(lower, a);
            upper = _mm_and_si128(upper, a);

            const __m128i pastNine = _mm_set1_epi8(10);
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

            ALIGNED_(16) std::array<char, 37> result;
            _mm_store_si128(reinterpret_cast<__m128i *>(result.data()), upperSorted);
            _mm_store_si128(reinterpret_cast<__m128i *>(result.data() + 16), lowerSorted);

            // Did not fit the last four chars. Extract and append them.
            const int v1 = _mm_extract_epi16(upper, 7);
            const int v2 = _mm_extract_epi16(lower, 7);
            result[32] = v1 & 0xff;
            result[33] = v2 & 0xff;
            result[34] = (v1 >> 8) & 0xff;
            result[35] = (v2 >> 8) & 0xff;
            result[36] = '\0';
            return result;
        }

        bool operator ==(UUID const& other) const noexcept
        {
            __m128i mm_left = _mm_load_si128(reinterpret_cast<const __m128i*>(_data));
            __m128i mm_right = _mm_load_si128(reinterpret_cast<const __m128i*>(other._data));
            // Very simple comparison. xor left and right and check if any of them are non-zero.
            return _mm_testz_si128(_mm_xor_si128(mm_left, mm_right), _mm_setzero_si128()) != 0;
        }

        bool operator !=(UUID const& other) const noexcept
        {
            return !(*this == other);
        }

    private:
        inline int innerParse(const char* ptr)
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
            if ((Tflag & VALIDATE_ALL) != 0)
            {
                ALIGNED_(16) const char _charRanges[17] = { '0', '9', 'A', 'Z', 'a', 'z', 0, -1, 0, -1, 0, -1, 0, -1, 0, -1 };
                const __m128i allowedCharRange = _mm_load_si128(reinterpret_cast<const __m128i*>(_charRanges));
                const int cmp = _mm_cmpistri(allowedCharRange, mask1, _SIDD_UBYTE_OPS | _SIDD_CMP_RANGES | _SIDD_NEGATIVE_POLARITY);
                const int cmp2 = _mm_cmpistri(allowedCharRange, mask2, _SIDD_UBYTE_OPS | _SIDD_CMP_RANGES | _SIDD_NEGATIVE_POLARITY);
                if (cmp != 16 || cmp2 != 16)
                    return 0;
            }

            const __m128i nine = _mm_set1_epi8('9');
            const __m128i aboveNineMask1 = _mm_cmpgt_epi8(mask1, nine);
            const __m128i aboveNineMask2 = _mm_cmpgt_epi8(mask2, nine);

            __m128i letterMask1 = _mm_and_si128(mask1, aboveNineMask1);
            __m128i letterMask2 = _mm_and_si128(mask2, aboveNineMask2);

            // Convert upper case when we are mixing upper with lower case
            if ((Tflag & CASE_UPPER) != 0 && (Tflag & CASE_LOWER) != 0)
            {
                // Convert upper case (A-Z) to lower case (a-z)
                const __m128i toLowerCase = _mm_set1_epi8(0x20);
                letterMask1 = _mm_or_si128(letterMask1, toLowerCase);
                letterMask2 = _mm_or_si128(letterMask2, toLowerCase);
            }

            const __m128i toHex = ((Tflag & CASE_UPPER) != 0) ? _mm_set1_epi8('A' - 10 - '0') : _mm_set1_epi8('a' - 10 - '0');

            const __m128i fixedUppercase1 = _mm_sub_epi8(letterMask1, toHex);
            const __m128i fixedUppercase2 = _mm_sub_epi8(letterMask2, toHex);
            mask1 = _mm_blendv_epi8(mask1, fixedUppercase1, aboveNineMask1);
            mask2 = _mm_blendv_epi8(mask2, fixedUppercase2, aboveNineMask2);

            const __m128i zero = _mm_set1_epi8('0');
            const __m128i lo = _mm_sub_epi8(mask2, zero);
            const __m128i hi = _mm_slli_epi16(_mm_sub_epi8(mask1, zero), 4);

            _mm_store_si128(reinterpret_cast<__m128i *>(_data), _mm_xor_si128(hi, lo));
            return 1;
        }

        ALIGNED_(16) unsigned char _data[16];
    };
}
