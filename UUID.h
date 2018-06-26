#pragma once

#include <cstdint>
#include <string_view>
#include <array>
#include <nmmintrin.h>

namespace meyr {
    class UUID final
    {
        static constexpr size_t m_len_compact = 32;
        static constexpr size_t m_len_hypens = 36;
        static constexpr size_t m_len_hypens_braces = 38;

        static constexpr size_t m_size = 16;

    public:
        UUID() = default;
        ~UUID() = default;

        UUID(UUID const& val)
        {
            _mm_store_si128(reinterpret_cast<__m128i *>(_uuid.data()), _mm_load_si128(reinterpret_cast<const __m128i *>(val._uuid.data())));
        }

        constexpr size_t size() const noexcept { return m_size; }

        bool try_parse(std::string_view str)
        {
            if (str.length() == m_len_compact) {
                return innerParse<false>(str.data());
            }
            else if (str.length() == m_len_hypens) {
                return innerParse<true>(str.data());
            }
            else if (str.length() == m_len_hypens_braces) {
                if ((str[0] == '(' && str[m_len_hypens_braces - 1] == ')') ||
                    (str[0] == '{' && str[m_len_hypens_braces - 1] == '}')) {
                    return innerParse<true>(str.data() + 1);
                }
            }
            return 0;
        }

        const std::array<unsigned char, m_size>& get_data()
        {
            return _uuid;
        }

        /*
            N 00000000000000000000000000000000
            D 00000000-0000-0000-0000-000000000000
            B {00000000-0000-0000-0000-000000000000}
            P (00000000-0000-0000-0000-000000000000)
        */
        std::string to_string(char format) const
        {
            bool useHypens = true;
            std::array<char, 2> braces;
            if (format == 'B' || format == 'b')
            {
                braces = { '{', '}' };
            }
            else if (format == 'F' || format == 'f')
            {
                braces = { '(', ')' };
            }
            else if (format == 'D' || format == 'd')
            {
                braces = { 0 };
            }
            else if (format == 'N' || format == 'n')
            {
                braces = { 0 };
                useHypens = false;
            }
            else
            {
                throw std::exception(); // todo
            }

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

            __m128i upperSorted;
            __m128i lowerSorted;
            if (useHypens)
            {
                const __m128i mask1 = _mm_shuffle_epi8(lower, _mm_setr_epi8(-1, 0, -1, 1, -1, 2, -1, 3, -1, -1, 4, -1, 5, -1, -1, 6));
                const __m128i mask2 = _mm_shuffle_epi8(upper, _mm_setr_epi8(0, -1, 1, -1, 2, -1, 3, -1, -1, 4, -1, 5, -1, -1, 6, -1));
                const __m128i mask3 = _mm_shuffle_epi8(lower, _mm_setr_epi8(-1, 7, -1, -1, 8, -1, 9, -1, -1, 10, -1, 11, -1, 12, -1, 13));
                const __m128i mask4 = _mm_shuffle_epi8(upper, _mm_setr_epi8(7, -1, -1, 8, -1, 9, -1, -1, 10, -1, 11, -1, 12, -1, 13, -1));
                const __m128i hypens = _mm_set_epi8(0, 0, '-', 0, 0, 0, 0, '-', 0, 0, 0, 0, 0, 0, 0, 0);
                const __m128i hypens2 = _mm_set_epi8(0, 0, 0, 0, 0, 0, 0, 0, '-', 0, 0, 0, 0, '-', 0, 0);
                upperSorted = _mm_or_si128(_mm_or_si128(mask1, mask2), hypens);
                lowerSorted = _mm_or_si128(_mm_or_si128(mask3, mask4), hypens2);
            }
            else
            {
                const __m128i mask1 = _mm_shuffle_epi8(lower, _mm_setr_epi8(-1, 0, -1, 1, -1, 2, -1, 3, -1, 4, -1, 5, -1, 6, -1, 7));
                const __m128i mask2 = _mm_shuffle_epi8(upper, _mm_setr_epi8(0, -1, 1, -1, 2, -1, 3, -1, 4, -1, 5, -1, 6, -1, 7, -1));
                const __m128i mask3 = _mm_shuffle_epi8(lower, _mm_setr_epi8(-1, 8, -1, 9, -1, 10, -1, 11, -1, 12, -1, 13, -1, 14, -1, 15));
                const __m128i mask4 = _mm_shuffle_epi8(upper, _mm_setr_epi8(8, -1, 9, -1, 10, -1, 11, -1, 12, -1, 13, -1, 14, -1, 15, -1));
                upperSorted = _mm_or_si128(mask1, mask2);
                lowerSorted = _mm_or_si128(mask3, mask4);
            }

            std::string result;
            if (braces[0] != 0)
            {
                result.reserve(m_len_hypens_braces);
                result.push_back(braces[0]);
            }
            else
            {
                if (useHypens)
                    result.reserve(m_len_hypens_braces);
                else
                    result.reserve(m_len_compact);
            }

            alignas(16) char temp[m_size * 2];
            _mm_store_si128(reinterpret_cast<__m128i *>(temp), upperSorted);
            _mm_store_si128(reinterpret_cast<__m128i *>(temp + m_size), lowerSorted);
            result.append(temp, m_size * 2);

            if (useHypens)
            {
                // Did not fit the last four chars. Extract and append them.
                const int v1 = _mm_extract_epi16(upper, 7);
                const int v2 = _mm_extract_epi16(lower, 7);
                result.push_back(v1 & 0xff);
                result.push_back(v2 & 0xff);
                result.push_back((v1 >> 8) & 0xff);
                result.push_back((v2 >> 8) & 0xff);
            }

            if (braces[1] != 0)
                result.push_back(braces[1]);

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
        inline bool innerParse(const char* ptr)
        {
            __m128i mm_lower_mask_1, mm_lower_mask_2, mm_upper_mask_1, mm_upper_mask_2;
            const __m128i mm_lower = _mm_loadu_si128(reinterpret_cast<const __m128i *>(ptr));

            if (includesHypens) {
                const __m128i mm_upper = _mm_loadu_si128(reinterpret_cast<const __m128i *>(ptr + m_size + 3));

                mm_lower_mask_1 = _mm_shuffle_epi8(mm_lower, _mm_setr_epi8(0, 2, 4, 6, 9, 11, 14, -1, -1, -1, -1, -1, -1, -1, -1, -1));
                mm_lower_mask_2 = _mm_shuffle_epi8(mm_lower, _mm_setr_epi8(1, 3, 5, 7, 10, 12, 15, -1, -1, -1, -1, -1, -1, -1, -1, -1));
                mm_upper_mask_1 = _mm_shuffle_epi8(mm_upper, _mm_setr_epi8(-1, -1, -1, -1, -1, -1, -1, -1, 0, 2, 5, 7, 9, 11, 13, -1));
                mm_upper_mask_2 = _mm_shuffle_epi8(mm_upper, _mm_setr_epi8(-1, -1, -1, -1, -1, -1, -1, -1, 1, 3, 6, 8, 10, 12, 14, -1));

                // Since we had hypens between the character we have 36 characters which does not fit in two 16 char loads
                // therefor we must manually insert them here
                mm_lower_mask_1 = _mm_insert_epi8(mm_lower_mask_1, ptr[16], 7);
                mm_lower_mask_2 = _mm_insert_epi8(mm_lower_mask_2, ptr[17], 7);
                mm_upper_mask_1 = _mm_insert_epi8(mm_upper_mask_1, ptr[34], 15);
                mm_upper_mask_2 = _mm_insert_epi8(mm_upper_mask_2, ptr[35], 15);
            }
            else {
                const __m128i mm_upper = _mm_loadu_si128(reinterpret_cast<const __m128i *>(ptr + m_size));

                mm_lower_mask_1 = _mm_shuffle_epi8(mm_lower, _mm_setr_epi8(0, 2, 4, 6, 8, 10, 12, 14, -1, -1, -1, -1, -1, -1, -1, -1));
                mm_lower_mask_2 = _mm_shuffle_epi8(mm_lower, _mm_setr_epi8(1, 3, 5, 7, 9, 11, 13, 15, -1, -1, -1, -1, -1, -1, -1, -1));
                mm_upper_mask_1 = _mm_shuffle_epi8(mm_upper, _mm_setr_epi8(-1, -1, -1, -1, -1, -1, -1, -1, 0, 2, 4, 6, 8, 10, 12, 14));
                mm_upper_mask_2 = _mm_shuffle_epi8(mm_upper, _mm_setr_epi8(-1, -1, -1, -1, -1, -1, -1, -1, 1, 3, 5, 7, 9, 11, 13, 15));
            }

            // Merge [aaaaaaaa|aaaaaaaa|00000000|00000000] | [00000000|00000000|bbbbbbbb|bbbbbbbb] -> [aaaaaaaa|aaaaaaaa|bbbbbbbb|bbbbbbbb]
            __m128i mm_mask_merge_1 = _mm_or_si128(mm_lower_mask_1, mm_upper_mask_1);
            __m128i mm_mask_merge_2 = _mm_or_si128(mm_lower_mask_2, mm_upper_mask_2);

            // Check if all characters are between 0-9, A-Z or a-z
            const __m128i mm_allowed_char_range = _mm_setr_epi8('0', '9', 'A', 'Z', 'a', 'z', 0, -1, 0, -1, 0, -1, 0, -1, 0, -1);
            const int cmp_lower = _mm_cmpistri(mm_allowed_char_range, mm_mask_merge_1, _SIDD_UBYTE_OPS | _SIDD_CMP_RANGES | _SIDD_NEGATIVE_POLARITY);
            const int cmp_upper = _mm_cmpistri(mm_allowed_char_range, mm_mask_merge_2, _SIDD_UBYTE_OPS | _SIDD_CMP_RANGES | _SIDD_NEGATIVE_POLARITY);
            if (cmp_lower != m_size || cmp_upper != m_size) {
                return false;
            }

            const __m128i nine = _mm_set1_epi8('9');
            const __m128i mm_above_nine_mask_1 = _mm_cmpgt_epi8(mm_mask_merge_1, nine);
            const __m128i mm_above_nine_mask_2 = _mm_cmpgt_epi8(mm_mask_merge_2, nine);

            __m128i mm_letter_mask_1 = _mm_and_si128(mm_mask_merge_1, mm_above_nine_mask_1);
            __m128i mm_letter_mask_2 = _mm_and_si128(mm_mask_merge_2, mm_above_nine_mask_2);

            // Convert all letters to to lower case first
            const __m128i toLowerCase = _mm_set1_epi8(0x20);
            mm_letter_mask_1 = _mm_or_si128(mm_letter_mask_1, toLowerCase);
            mm_letter_mask_2 = _mm_or_si128(mm_letter_mask_2, toLowerCase);

            // now convert to hex
            const __m128i toHex = _mm_set1_epi8('a' - 10 - '0');
            const __m128i fixedUppercase1 = _mm_sub_epi8(mm_letter_mask_1, toHex);
            const __m128i fixedUppercase2 = _mm_sub_epi8(mm_letter_mask_2, toHex);

            const __m128i mm_blended_high = _mm_blendv_epi8(mm_mask_merge_1, fixedUppercase1, mm_above_nine_mask_1);
            const __m128i mm_blended_low = _mm_blendv_epi8(mm_mask_merge_2, fixedUppercase2, mm_above_nine_mask_2);
            const __m128i zero = _mm_set1_epi8('0');
            __m128i lo = _mm_sub_epi8(mm_blended_low, zero);
            __m128i hi = _mm_sub_epi8(mm_blended_high, zero);
            hi = _mm_slli_epi16(hi, 4);

            _mm_store_si128(reinterpret_cast<__m128i *>(_uuid.data()), _mm_xor_si128(hi, lo));
            return true;
        }

        alignas(16) std::array<unsigned char, m_size> _uuid;
    };
}
