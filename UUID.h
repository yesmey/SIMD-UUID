#pragma once

#include <cstdint>
#include <string_view>
#include <array>
#include <nmmintrin.h>

namespace meyr {
    class UUID final
    {
    public:
        UUID() = default;
        ~UUID() = default;

        UUID(UUID const& val)
        {
            _mm_store_si128(reinterpret_cast<__m128i *>(_uuid.data()), _mm_load_si128(reinterpret_cast<const __m128i *>(val._uuid.data())));
        }

        int parse(std::string_view str)
        {
            if (str.length() == 32) {
                return innerParse<false>(str.data());
            }
            else if (str.length() == 36) {
                return innerParse<true>(str.data());
            }
            else if (str.length() == 38) {
                if ((str[0] == '(' && str[37] == ')') || (str[0] == '{' && str[37] == '}')) {
                    return innerParse<true>(str.data() + 1);
                }
            }
            return 0;
        }

        const std::array<unsigned char, 16>& get_data()
        {
            return _uuid;
        }

        /*
            D 00000000-0000-0000-0000-000000000000
            B {00000000-0000-0000-0000-000000000000}
            P (00000000-0000-0000-0000-000000000000)
        */
        std::string to_string(char format) const
        {
            __m128i lower = _mm_load_si128(reinterpret_cast<const __m128i*>(_uuid.data()));
            __m128i upper = _mm_and_si128(_mm_set1_epi8(0xFF >> 4), _mm_srli_epi32(lower, 4));

            const __m128i a = _mm_set1_epi8(0x0F);
            lower = _mm_and_si128(lower, a);
            upper = _mm_and_si128(upper, a);

            const __m128i pastNine = _mm_set1_epi8(9 + 1);
            const __m128i lowerMask = _mm_cmplt_epi8(lower, pastNine);
            const __m128i upperMask = _mm_cmplt_epi8(upper, pastNine);

            __m128i letterMask1 = _mm_and_si128(lower, lowerMask);
            __m128i letterMask2 = _mm_and_si128(upper, upperMask);
            __m128i letterMask3 = _mm_or_si128(lower, lowerMask);
            __m128i letterMask4 = _mm_or_si128(upper, upperMask);

            const __m128i first = _mm_set1_epi8('0');
            const __m128i second = _mm_set1_epi8(((format & 0x20) == 0 ? 'A' : 'a') - 10);

            letterMask1 = _mm_add_epi8(letterMask1, first);
            letterMask2 = _mm_add_epi8(letterMask2, first);
            letterMask3 = _mm_add_epi8(letterMask3, second);
            letterMask4 = _mm_add_epi8(letterMask4, second);

            lower = _mm_blendv_epi8(letterMask3, letterMask1, lowerMask);
            upper = _mm_blendv_epi8(letterMask4, letterMask2, upperMask);

            const __m128i MASK_1 = _mm_setr_epi8(-1, 0, -1, 1, -1, 2, -1, 3, -1, -1, 4, -1, 5, -1, -1, 6);
            const __m128i MASK_2 = _mm_setr_epi8(0, -1, 1, -1, 2, -1, 3, -1, -1, 4, -1, 5, -1, -1, 6, -1);
            const __m128i MASK_3 = _mm_setr_epi8(-1, 7, -1, -1, 8, -1, 9, -1, -1, 10, -1, 11, -1, 12, -1, 13);
            const __m128i MASK_4 = _mm_setr_epi8(7, -1, -1, 8, -1, 9, -1, -1, 10, -1, 11, -1, 12, -1, 13, -1);

            const __m128i mask1 = _mm_shuffle_epi8(lower, MASK_1);
            const __m128i mask2 = _mm_shuffle_epi8(upper, MASK_2);
            const __m128i mask3 = _mm_shuffle_epi8(lower, MASK_3);
            const __m128i mask4 = _mm_shuffle_epi8(upper, MASK_4);

            const __m128i bind = _mm_set_epi8(0, 0, '-', 0, 0, 0, 0, '-', 0, 0, 0, 0, 0, 0, 0, 0);
            const __m128i bind2 = _mm_set_epi8(0, 0, 0, 0, 0, 0, 0, 0, '-', 0, 0, 0, 0, '-', 0, 0);

            const __m128i upperSorted = _mm_or_si128(_mm_or_si128(mask1, mask2), bind);
            const __m128i lowerSorted = _mm_or_si128(_mm_or_si128(mask3, mask4), bind2);

            std::string result;
            char* dataPtr;

            if (format == 'B' || format == 'b')
            {
                result.reserve(39);
                dataPtr = result.data() + 1;
                dataPtr[0] = '{';
                dataPtr[36] = '}';
                dataPtr[37] = '\0';
            }
            else if (format == 'F' || format == 'f')
            {
                result.reserve(39);
                dataPtr = result.data() + 1;
                dataPtr[0] = '(';
                dataPtr[36] = ')';
                dataPtr[37] = '\0';
            }
            else
            {
                result.reserve(37);
                dataPtr = result.data();
                dataPtr[36] = '\0';
            }

            _mm_storeu_si128(reinterpret_cast<__m128i *>(dataPtr), upperSorted);
            _mm_storeu_si128(reinterpret_cast<__m128i *>(dataPtr + 16), lowerSorted);

            // Did not fit the last four chars. Extract and append them.
            const int v1 = _mm_extract_epi16(upper, 7);
            const int v2 = _mm_extract_epi16(lower, 7);
            dataPtr[32] = v1 & 0xff;
            dataPtr[33] = v2 & 0xff;
            dataPtr[34] = (v1 >> 8) & 0xff;
            dataPtr[35] = (v2 >> 8) & 0xff;
            return result;
        }

        bool operator ==(UUID const& other) const noexcept
        {
            __m128i mm_left = _mm_load_si128(reinterpret_cast<const __m128i*>(_uuid.data()));
            __m128i mm_right = _mm_load_si128(reinterpret_cast<const __m128i*>(other._uuid.data()));
            // Very simple comparison. xor left and right and check if any of them are non-zero.
            return _mm_testz_si128(_mm_xor_si128(mm_left, mm_right), _mm_setzero_si128()) != 0;
        }

        bool operator !=(UUID const& other) const noexcept
        {
            return !(*this == other);
        }

    private:
        template<bool includesHypens>
        inline int innerParse(const char* ptr)
        {
            __m128i mask1, mask2, mask3, mask4;
            const __m128i str = _mm_loadu_si128(reinterpret_cast<const __m128i *>(ptr));

            if (includesHypens) {
                const __m128i str2 = _mm_loadu_si128(reinterpret_cast<const __m128i *>(ptr + 19));

                mask1 = _mm_shuffle_epi8(str, _mm_setr_epi8(0, 2, 4, 6, 9, 11, 14, -1, -1, -1, -1, -1, -1, -1, -1, -1));
                mask2 = _mm_shuffle_epi8(str, _mm_setr_epi8(1, 3, 5, 7, 10, 12, 15, -1, -1, -1, -1, -1, -1, -1, -1, -1));
                mask3 = _mm_shuffle_epi8(str2, _mm_setr_epi8(-1, -1, -1, -1, -1, -1, -1, -1, 0, 2, 5, 7, 9, 11, 13, -1));
                mask4 = _mm_shuffle_epi8(str2, _mm_setr_epi8(-1, -1, -1, -1, -1, -1, -1, -1, 1, 3, 6, 8, 10, 12, 14, -1));

                // Since we had hypens between the character we have 36 characters which does not fit in two 16 char loads
                // therefor we must manually insert them here
                mask1 = _mm_insert_epi8(mask1, ptr[16], 7);
                mask2 = _mm_insert_epi8(mask2, ptr[17], 7);
                mask3 = _mm_insert_epi8(mask3, ptr[34], 15);
                mask4 = _mm_insert_epi8(mask4, ptr[35], 15);
            }
            else {
                const __m128i str2 = _mm_loadu_si128(reinterpret_cast<const __m128i *>(ptr + 16));

                mask1 = _mm_shuffle_epi8(str, _mm_setr_epi8(0, 2, 4, 6, 8, 10, 12, 14, -1, -1, -1, -1, -1, -1, -1, -1));
                mask2 = _mm_shuffle_epi8(str, _mm_setr_epi8(1, 3, 5, 7, 9, 11, 13, 15, -1, -1, -1, -1, -1, -1, -1, -1));
                mask3 = _mm_shuffle_epi8(str2, _mm_setr_epi8(-1, -1, -1, -1, -1, -1, -1, -1, 0, 2, 4, 6, 8, 10, 12, 14));
                mask4 = _mm_shuffle_epi8(str2, _mm_setr_epi8(-1, -1, -1, -1, -1, -1, -1, -1, 1, 3, 5, 7, 9, 11, 13, 15));
            }

            // Merge [aaaaaaaa|aaaaaaaa|00000000|00000000] | [00000000|00000000|bbbbbbbb|bbbbbbbb]
            mask1 = _mm_or_si128(mask1, mask3);
            mask2 = _mm_or_si128(mask2, mask4);

            // Check if all characters are between 0-9, A-Z or a-z
            const __m128i allowedCharRange = _mm_setr_epi8('0', '9', 'A', 'Z', 'a', 'z', 0, -1, 0, -1, 0, -1, 0, -1, 0, -1);
            const int cmp = _mm_cmpistri(allowedCharRange, mask1, _SIDD_UBYTE_OPS | _SIDD_CMP_RANGES | _SIDD_NEGATIVE_POLARITY);
            const int cmp2 = _mm_cmpistri(allowedCharRange, mask2, _SIDD_UBYTE_OPS | _SIDD_CMP_RANGES | _SIDD_NEGATIVE_POLARITY);
            if (cmp != 16 || cmp2 != 16)
                return 0;

            const __m128i nine = _mm_set1_epi8('9');
            const __m128i aboveNineMask1 = _mm_cmpgt_epi8(mask1, nine);
            const __m128i aboveNineMask2 = _mm_cmpgt_epi8(mask2, nine);

            __m128i letterMask1 = _mm_and_si128(mask1, aboveNineMask1);
            __m128i letterMask2 = _mm_and_si128(mask2, aboveNineMask2);

            // Convert all letters to to lower case first
            const __m128i toLowerCase = _mm_set1_epi8(0x20);
            letterMask1 = _mm_or_si128(letterMask1, toLowerCase);
            letterMask2 = _mm_or_si128(letterMask2, toLowerCase);

            // now convert to hex
            const __m128i toHex = _mm_set1_epi8('a' - 10 - '0');
            const __m128i fixedUppercase1 = _mm_sub_epi8(letterMask1, toHex);
            const __m128i fixedUppercase2 = _mm_sub_epi8(letterMask2, toHex);

            mask1 = _mm_blendv_epi8(mask1, fixedUppercase1, aboveNineMask1);
            mask2 = _mm_blendv_epi8(mask2, fixedUppercase2, aboveNineMask2);
            const __m128i zero = _mm_set1_epi8('0');
            const __m128i lo = _mm_sub_epi8(mask2, zero);
            const __m128i hi = _mm_slli_epi16(_mm_sub_epi8(mask1, zero), 4);

            _mm_store_si128(reinterpret_cast<__m128i *>(_uuid.data()), _mm_xor_si128(hi, lo));
            return 1;
        }

        alignas(16) std::array<unsigned char, 16> _uuid;
    };
}
