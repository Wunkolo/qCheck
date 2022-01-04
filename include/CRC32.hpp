#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

#ifdef __AVX2__
#include <immintrin.h>
#endif

namespace CRC
{

enum class Polynomial : std::uint32_t
{
	CRC32   = 0xEDB88320,
	CRC32C  = 0x82F63B78,
	CRC32K  = 0xEB31D82E,
	CRC32K2 = 0x992C1A4C,
	CRC32Q  = 0xD5828281,
};

// TODO: This is pretty big, about 16kb total, which is almost
// half of most processor's data cache(32kb)
// If we use a LUT-less carryless multiply instruction, then the cache can
// be much more properly populated with actual IO data rather than our LUT
// https://www.intel.com/content/dam/www/public/us/en/documents/white-papers/fast-crc-computation-generic-polynomials-pclmulqdq-paper.pdf
// - Tue 30 Jun 2020 12:04:40 PM PDT
constexpr std::array<std::array<std::uint32_t, 256>, 16>
	CRC32Table(std::uint32_t Polynomial) noexcept
{
	std::array<std::array<std::uint32_t, 256>, 16> Table = {};
	// Generate main table
	for( std::size_t i = 0; i < 256; ++i )
	{
		std::uint32_t CRC = i;
		for( std::size_t CurBit = 0; CurBit < 8; ++CurBit )
		{
			CRC = (CRC >> 1) ^ (-(CRC & 0b1) & Polynomial);
		}
		Table[0][i] = CRC;
	}
	// Generate additional tables based on the main table
	for( std::size_t i = 0; i < 256; ++i )
	{
		Table[1][i]  = (Table[0][i] >> 8) ^ Table[0][std::uint8_t(Table[0][i])];
		Table[2][i]  = (Table[1][i] >> 8) ^ Table[0][std::uint8_t(Table[1][i])];
		Table[3][i]  = (Table[2][i] >> 8) ^ Table[0][std::uint8_t(Table[2][i])];
		Table[4][i]  = (Table[3][i] >> 8) ^ Table[0][std::uint8_t(Table[3][i])];
		Table[5][i]  = (Table[4][i] >> 8) ^ Table[0][std::uint8_t(Table[4][i])];
		Table[6][i]  = (Table[5][i] >> 8) ^ Table[0][std::uint8_t(Table[5][i])];
		Table[7][i]  = (Table[6][i] >> 8) ^ Table[0][std::uint8_t(Table[6][i])];
		Table[8][i]  = (Table[7][i] >> 8) ^ Table[0][std::uint8_t(Table[7][i])];
		Table[9][i]  = (Table[8][i] >> 8) ^ Table[0][std::uint8_t(Table[8][i])];
		Table[10][i] = (Table[9][i] >> 8) ^ Table[0][std::uint8_t(Table[9][i])];
		Table[11][i]
			= (Table[10][i] >> 8) ^ Table[0][std::uint8_t(Table[10][i])];
		Table[12][i]
			= (Table[11][i] >> 8) ^ Table[0][std::uint8_t(Table[11][i])];
		Table[13][i]
			= (Table[12][i] >> 8) ^ Table[0][std::uint8_t(Table[12][i])];
		Table[14][i]
			= (Table[13][i] >> 8) ^ Table[0][std::uint8_t(Table[13][i])];
		Table[15][i]
			= (Table[14][i] >> 8) ^ Table[0][std::uint8_t(Table[14][i])];
	}

	return Table;
}

#ifdef __AVX2__
inline std::uint32_t _mm256_hxor_epi32(__m256i a)
{
	// Xor top half with bottom half
	const __m128i XorReduce128 = _mm_xor_si128(
		_mm256_extracti128_si256(a, 1), _mm256_castsi256_si128(a));
	const std::uint64_t XorReduce64 = _mm_extract_epi64(XorReduce128, 1)
									^ _mm_extract_epi64(XorReduce128, 0);
	return XorReduce64 ^ (XorReduce64 >> 32);
}
#endif

// Todo: This should technically be "contiguous_iterator_tag" from C++20
template<typename RandomIterator, std::uint32_t Polynomial>
std::uint32_t Checksum(
	RandomIterator First, RandomIterator Last, std::random_access_iterator_tag)
{
	static constexpr auto Table = CRC32Table(Polynomial);
	std::uint32_t         CRC   = ~0;
	// const std::size_t Size = static_cast<std::size_t>(std::distance(First,
	// Last));
	const std::uint32_t* Input32
		= reinterpret_cast<const std::uint32_t*>(&(*First));

	// Slice by 16
	{
#if defined(__AVX512F__) && defined(__AVX512BW__)
		const __m512i ByteIndex = _mm512_set_epi8(
			~0, ~0, ~0, 15, ~0, ~0, ~0, 14, ~0, ~0, ~0, 13, ~0, ~0, ~0, 12, ~0,
			~0, ~0, 11, ~0, ~0, ~0, 10, ~0, ~0, ~0, 9, ~0, ~0, ~0, 8, ~0, ~0,
			~0, 7, ~0, ~0, ~0, 6, ~0, ~0, ~0, 5, ~0, ~0, ~0, 4, ~0, ~0, ~0, 3,
			~0, ~0, ~0, 2, ~0, ~0, ~0, 1, ~0, ~0, ~0, 0);
		// Offset into the multi-dimensional array
		const __m512i ArrayOffset = _mm512_set_epi32(
			(sizeof(typename decltype(Table)::value_type) / 4) * 0,
			(sizeof(typename decltype(Table)::value_type) / 4) * 1,
			(sizeof(typename decltype(Table)::value_type) / 4) * 2,
			(sizeof(typename decltype(Table)::value_type) / 4) * 3,
			(sizeof(typename decltype(Table)::value_type) / 4) * 4,
			(sizeof(typename decltype(Table)::value_type) / 4) * 5,
			(sizeof(typename decltype(Table)::value_type) / 4) * 6,
			(sizeof(typename decltype(Table)::value_type) / 4) * 7,
			(sizeof(typename decltype(Table)::value_type) / 4) * 8,
			(sizeof(typename decltype(Table)::value_type) / 4) * 9,
			(sizeof(typename decltype(Table)::value_type) / 4) * 10,
			(sizeof(typename decltype(Table)::value_type) / 4) * 11,
			(sizeof(typename decltype(Table)::value_type) / 4) * 12,
			(sizeof(typename decltype(Table)::value_type) / 4) * 13,
			(sizeof(typename decltype(Table)::value_type) / 4) * 14,
			(sizeof(typename decltype(Table)::value_type) / 4) * 15);
		for( ; std::distance(First, Last) / 16; First += 16 )
		{
			// Load in 8 bytes
			const std::uint64_t Input64Lo
				= *reinterpret_cast<const std::uint64_t*>(Input32) ^ CRC;
			Input32 += 2;
			const std::uint64_t Input64Hi
				= *reinterpret_cast<const std::uint64_t*>(Input32);
			Input32 += 2;
			// Spread out each byte into a eight 32-bit lanes, in each 256-bit
			// lane
			const __m512i Indices = _mm512_shuffle_epi8(
				_mm512_unpacklo_epi64(
					_mm512_set1_epi64(Input64Lo), _mm512_set1_epi64(Input64Hi)),
				ByteIndex);
			// Use the spread out bytes to gather
			const __m512i Gather = _mm512_i32gather_epi32(
				_mm512_add_epi32(Indices, ArrayOffset),
				reinterpret_cast<const std::int32_t*>(Table.data()),
				sizeof(std::uint32_t));
			CRC = _mm256_hxor_epi32(_mm256_xor_si256(
				_mm512_castsi512_si256(Gather),
				_mm512_extracti32x8_epi32(Gather, 1)));
		}
#else
		for( ; std::distance(First, Last) / 16; First += 16 )
		{
			const std::uint32_t InputLoLo = *Input32++ ^ CRC;
			const std::uint32_t InputHiLo = *Input32++;
			const std::uint32_t InputLoHi = *Input32++;
			const std::uint32_t InputHiHi = *Input32++;
			CRC                           = Table[15][std::uint8_t(InputLoLo)]
				^ Table[14][std::uint8_t(InputLoLo >> 8)]
				^ Table[13][std::uint8_t(InputLoLo >> 16)]
				^ Table[12][std::uint8_t(InputLoLo >> 24)]
				^ Table[11][std::uint8_t(InputHiLo)]
				^ Table[10][std::uint8_t(InputHiLo >> 8)]
				^ Table[9][std::uint8_t(InputHiLo >> 16)]
				^ Table[8][std::uint8_t(InputHiLo >> 24)]
				^ Table[7][std::uint8_t(InputLoHi)]
				^ Table[6][std::uint8_t(InputLoHi >> 8)]
				^ Table[5][std::uint8_t(InputLoHi >> 16)]
				^ Table[4][std::uint8_t(InputLoHi >> 24)]
				^ Table[3][std::uint8_t(InputHiHi)]
				^ Table[2][std::uint8_t(InputHiHi >> 8)]
				^ Table[1][std::uint8_t(InputHiHi >> 16)]
				^ Table[0][std::uint8_t(InputHiHi >> 24)];
		}
#endif
	}

	// Slice by 8
	{
#if defined(__AVX2__)
		const __m256i ByteIndex = _mm256_set_epi8(
			~0, ~0, ~0, 7, ~0, ~0, ~0, 6, ~0, ~0, ~0, 5, ~0, ~0, ~0, 4, ~0, ~0,
			~0, 3, ~0, ~0, ~0, 2, ~0, ~0, ~0, 1, ~0, ~0, ~0, 0);
		// Offset into the multi-dimensional array
		const __m256i ArrayOffset = _mm256_set_epi32(
			(sizeof(typename decltype(Table)::value_type) / 4) * 0,
			(sizeof(typename decltype(Table)::value_type) / 4) * 1,
			(sizeof(typename decltype(Table)::value_type) / 4) * 2,
			(sizeof(typename decltype(Table)::value_type) / 4) * 3,
			(sizeof(typename decltype(Table)::value_type) / 4) * 4,
			(sizeof(typename decltype(Table)::value_type) / 4) * 5,
			(sizeof(typename decltype(Table)::value_type) / 4) * 6,
			(sizeof(typename decltype(Table)::value_type) / 4) * 7);
		for( ; std::distance(First, Last) / 8; First += 8 )
		{
			// Load in 8 bytes
			const std::uint64_t Input64
				= *reinterpret_cast<const std::uint64_t*>(Input32) ^ CRC;
			Input32 += 2;
			// Spread out each byte into a eight 32-bit lanes
			const __m256i Indices
				= _mm256_shuffle_epi8(_mm256_set1_epi64x(Input64), ByteIndex);
			// Use the spread out bytes to gather
			const __m256i Gather = _mm256_i32gather_epi32(
				reinterpret_cast<const std::int32_t*>(Table.data()),
				_mm256_add_epi32(Indices, ArrayOffset), sizeof(std::uint32_t));
			CRC = _mm256_hxor_epi32(Gather);
		}
#else
		for( ; std::distance(First, Last) / 8; First += 8 )
		{
			const std::uint32_t InputLo = *Input32++ ^ CRC;
			const std::uint32_t InputHi = *Input32++;
			CRC                         = Table[7][std::uint8_t(InputLo)]
				^ Table[6][std::uint8_t(InputLo >> 8)]
				^ Table[5][std::uint8_t(InputLo >> 16)]
				^ Table[4][std::uint8_t(InputLo >> 24)]
				^ Table[3][std::uint8_t(InputHi)]
				^ Table[2][std::uint8_t(InputHi >> 8)]
				^ Table[1][std::uint8_t(InputHi >> 16)]
				^ Table[0][std::uint8_t(InputHi >> 24)];
		}
#endif
	}

	return ~std::accumulate(
		First, Last, CRC, [](std::uint32_t CRC, std::uint8_t Byte) {
			return (CRC >> 8) ^ Table[0][std::uint8_t(CRC) ^ Byte];
		});
}

template<typename Iterator, std::uint32_t Polynomial>
std::uint32_t Checksum(Iterator First, Iterator Last, std::input_iterator_tag)
{
	static constexpr auto Table = CRC32Table(Polynomial);
	return ~std::accumulate(
		First, Last, ~std::uint32_t(0),
		[](std::uint32_t CRC, std::uint8_t Byte) {
			return (CRC >> 8) ^ Table[0][std::uint8_t(CRC) ^ Byte];
		});
}

template<std::uint32_t Polynomial, typename Iterator>
inline std::uint32_t Checksum(Iterator First, Iterator Last)
{
	return Checksum<Iterator, Polynomial>(
		First, Last,
		typename std::iterator_traits<Iterator>::iterator_category());
}
} // namespace CRC