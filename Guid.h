#pragma once
#include <cstdint>
#include <smmintrin.h>

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

	int Parse(const char* ptr)
	{
		// 5%
		if (ptr[8] != '-' || ptr[13] != '-' || ptr[18] != '-' || ptr[23] != '-')
		{
			return -1;
		}

		const __m128i zero = loadSimdCharArray('0');
		const __m128i nine = loadSimdCharArray('9');
		const __m128i str = _mm_loadu_si128(reinterpret_cast<const __m128i *>(ptr));
		const __m128i str2 = _mm_loadu_si128(reinterpret_cast<const __m128i *>(ptr + 19));

		// 10%
		if (_mm_movemask_epi8(_mm_cmplt_epi8(str, zero)) != 8448 ||
			_mm_movemask_epi8(_mm_cmplt_epi8(str2, zero)) != 16)
			return -1;

		const char& ch16 = (ptr[16] << 8) | ptr[16];
		const char& ch17 = (ptr[17] << 8) | ptr[17];
		const char& ch34 = (ptr[34] << 8) | ptr[34];
		const char& ch35 = (ptr[35] << 8) | ptr[35];

		// [aacceegg|jjllooqq]
		// [bbddffhh|kkmmpprr]
		__m128i hi, lo, hi2, lo2;

		{
			const auto MASK_1 = _mm_setr_epi8(
				0, 0, 2, 2,
				4, 4, 6, 6,
				9, 9, 11, 11,
				14, 14, -1, -1);

			const auto MASK_2 = _mm_setr_epi8(
				1, 1, 3, 3,
				5, 5, 7, 7,
				10, 10, 12, 12,
				15, 15, -1, -1);

			const auto MASK_3 = _mm_setr_epi8(
				0, 0, 2, 2,
				5, 5, 7, 7,
				9, 9, 11, 11,
				13, 13, -1, -1);

			const auto MASK_4 = _mm_setr_epi8(
				1, 1, 3, 3,
				6, 6, 8, 8,
				10, 10, 12, 12,
				14, 14, -1, -1);

			__m128i mask1 = _mm_shuffle_epi8(str, MASK_1);
			__m128i mask2 = _mm_shuffle_epi8(str, MASK_2);
			__m128i mask3 = _mm_shuffle_epi8(str2, MASK_3);
			__m128i mask4 = _mm_shuffle_epi8(str2, MASK_4);

			// 15 %
			mask1 = _mm_insert_epi16(mask1, ch16, 7);
			mask2 = _mm_insert_epi16(mask2, ch17, 7);
			mask3 = _mm_insert_epi16(mask3, ch34, 7);
			mask4 = _mm_insert_epi16(mask4, ch35, 7);

			{
				const auto toLowerCase = loadSimdCharArray(0x20);
				const auto toHex = loadSimdCharArray('a' - 10 - '0');

				const auto aboveNineMask1 = _mm_cmpgt_epi8(mask1, nine);
				const auto aboveNineMask2 = _mm_cmpgt_epi8(mask2, nine);
				const auto aboveNineMask3 = _mm_cmpgt_epi8(mask3, nine);
				const auto aboveNineMask4 = _mm_cmpgt_epi8(mask4, nine);

				const auto and1 = _mm_and_si128(mask1, aboveNineMask1);
				const auto and2 = _mm_and_si128(mask2, aboveNineMask2);
				const auto and3 = _mm_and_si128(mask3, aboveNineMask3);
				const auto and4 = _mm_and_si128(mask4, aboveNineMask4);

				const auto or1 = _mm_or_si128(and1, toLowerCase);
				const auto or2 = _mm_or_si128(and2, toLowerCase);
				const auto or3 = _mm_or_si128(and3, toLowerCase);
				const auto or4 = _mm_or_si128(and4, toLowerCase);

				const auto fixedUppercase1 = _mm_sub_epi8(or1, toHex);
				const auto fixedUppercase2 = _mm_sub_epi8(or2, toHex);
				const auto fixedUppercase3 = _mm_sub_epi8(or3, toHex);
				const auto fixedUppercase4 = _mm_sub_epi8(or4, toHex);

				mask1 = _mm_blendv_epi8(mask1, fixedUppercase1, aboveNineMask1);
				mask2 = _mm_blendv_epi8(mask2, fixedUppercase2, aboveNineMask2);
				mask3 = _mm_blendv_epi8(mask3, fixedUppercase3, aboveNineMask3);
				mask4 = _mm_blendv_epi8(mask4, fixedUppercase4, aboveNineMask4);
			}

			hi = _mm_sub_epi8(mask1, zero);
			lo = _mm_sub_epi8(mask2, zero);
			hi2 = _mm_sub_epi8(mask3, zero);
			lo2 = _mm_sub_epi8(mask4, zero);

			// 8%
			hi = _mm_slli_epi16(hi, 4);
			hi2 = _mm_slli_epi16(hi2, 4);
		}

		auto result = _mm_shuffle_epi8(_mm_xor_si128(hi, lo), _mm_setr_epi8(
			6, 4, 2, 0,
			10, 8, 14, 12,
			-1, -1, -1, -1,
			-1, -1, -1, -1));

		const auto result2 = _mm_shuffle_epi8(_mm_xor_si128(hi2, lo2), _mm_setr_epi8(
			-1, -1, -1, -1,
			-1, -1, -1, -1,
			0, 2, 4, 6,
			8, 10, 12, 14));

		result = _mm_or_si128(result, result2);

		// 5%
		_mm_store_si128(reinterpret_cast<__m128i *>(&_uuid.data[0]), result);
		return 0;
	}

private:
	union alignas(16) uuid
	{
		unsigned char data[16];
		uint64_t ulongs[2];
		uint32_t uints[4];
	};

	uuid _uuid;
};
