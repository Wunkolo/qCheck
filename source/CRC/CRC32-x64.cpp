#if defined(_M_X64) || defined(__amd64__)

#include <CRC32.hpp>

namespace CRC
{

using CRC32TableT = std::array<std::array<std::uint32_t, 256>, 16>;

constexpr CRC32TableT CRC32Table(std::uint32_t Polynomial) noexcept
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

template<Polynomial Poly>
struct CRC32TableStatic
{
	const CRC32TableT& operator()() const
	{
		static constexpr CRC32TableT Table = CRC32Table(std::uint32_t(Poly));
		return Table;
	}
};

static const CRC32TableT& GetCRC32Table(Polynomial Poly)
{
	switch( Poly )
	{
	default:
	case Polynomial::CRC32:
		return CRC32TableStatic<Polynomial::CRC32>()();
	case Polynomial::CRC32C:
		return CRC32TableStatic<Polynomial::CRC32C>()();
	case Polynomial::CRC32K:
		return CRC32TableStatic<Polynomial::CRC32K>()();
	case Polynomial::CRC32K2:
		return CRC32TableStatic<Polynomial::CRC32K2>()();
	case Polynomial::CRC32Q:
		return CRC32TableStatic<Polynomial::CRC32Q>()();
	}
}

consteval std::uint32_t BitReverse32(std::uint32_t Value)
{
	std::uint32_t Reversed = 0;
	for( std::uint32_t BitIndex = 0u; BitIndex < 32u; ++BitIndex )
	{
		Reversed = (Reversed << 1u) + (Value & 0b1);
		Value >>= 1u;
	}
	return Reversed;
}

// BitReverse(x^(shift) mod P(x) << 32) << 1
consteval std::uint64_t
	KnConstant(std::uint32_t ByteShift, std::uint32_t Polynomial)
{
	std::uint32_t Remainder = 1u << 24;
	for( std::size_t i = 5; i <= (ByteShift + 1); ++i )
	{
		for( std::int8_t BitIndex = 0; BitIndex < 8; ++BitIndex )
		{
			// Remainder is about to overflow, increment quotient
			if( Remainder >> 31u )
			{
				Remainder <<= 1u;        // r *= x
				Remainder ^= Polynomial; // r += poly
			}
			else
			{
				Remainder <<= 1u; // r *= 2
			}
		}
	}
	return static_cast<std::uint64_t>(BitReverse32(Remainder)) << 1;
}

// BitReverse(x^64 / P(x)) << 1
consteval std::uint64_t MuConstant(uint32_t Polynomial)
{
	std::uint32_t Remainder = 1u << 24;
	std::uint32_t Quotient  = 0u;
	for( std::size_t i = 5; i <= 9; ++i )
	{
		for( std::int8_t BitIndex = 0; BitIndex < 8; ++BitIndex )
		{
			Quotient <<= 1u; // q *= x
			// Remainder is about to overflow, increment quotient
			if( Remainder >> 31u )
			{
				Remainder <<= 1u;        // r *= x
				Remainder ^= Polynomial; // + poly
				Quotient |= 1u;          // + x^0
			}
			else
			{
				Remainder <<= 1u; // r *= 2
			}
		}
	}
	return (static_cast<std::uint64_t>(BitReverse32(Quotient)) << 1u) | 1;
}

#define POLY 0x1EDC6F41
#define IEEEPOLY 0x04C11DB7

// clang-format off
static_assert(KnConstant( 64 + 4, IEEEPOLY) == 0x154442BD4);
static_assert(KnConstant( 64 - 4, IEEEPOLY) == 0x1C6E41596);
static_assert(KnConstant( 16 + 4, IEEEPOLY) == 0x1751997D0);
static_assert(KnConstant( 16 - 4, IEEEPOLY) == 0x0CCAA009E);
static_assert(KnConstant(      8, IEEEPOLY) == 0x163CD6124);
static_assert(KnConstant(      4, IEEEPOLY) == 0x1DB710640);
static_assert(MuConstant(         IEEEPOLY) == 0x1F7011641);
// clang-format on

#ifdef __PCLMUL__
template<std::uint32_t Polynomial>
std::uint32_t
	CRC32_PCLMULQDQ(std::span<const std::byte> Data, std::uint32_t CRC)
{

	__m128i CRCVec0
		= _mm_loadu_si128(&reinterpret_cast<const __m128i*>(Data.data())[0]);
	__m128i CRCVec1
		= _mm_loadu_si128(&reinterpret_cast<const __m128i*>(Data.data())[1]);
	__m128i CRCVec2
		= _mm_loadu_si128(&reinterpret_cast<const __m128i*>(Data.data())[2]);
	__m128i CRCVec3
		= _mm_loadu_si128(&reinterpret_cast<const __m128i*>(Data.data())[3]);

	CRCVec0 = _mm_xor_si128(CRCVec0, _mm_cvtsi32_si128(CRC));

	Data = Data.subspan(64);

	// Fold 512 bits at a time
#if defined(__AVX512F__) && defined(__AVX512VL__) && defined(__VPCLMULQDQ__)
	{
		__m512i CRCVec_512 = _mm512_undefined();

		CRCVec_512 = _mm512_inserti32x4(CRCVec_512, CRCVec0, 0);
		CRCVec_512 = _mm512_inserti32x4(CRCVec_512, CRCVec1, 1);
		CRCVec_512 = _mm512_inserti32x4(CRCVec_512, CRCVec2, 2);
		CRCVec_512 = _mm512_inserti32x4(CRCVec_512, CRCVec3, 3);

		const __m512i K1K2 = _mm512_set_epi64(
			KnConstant(64 - 4, Polynomial), KnConstant(64 + 4, Polynomial),
			KnConstant(64 - 4, Polynomial), KnConstant(64 + 4, Polynomial),
			KnConstant(64 - 4, Polynomial), KnConstant(64 + 4, Polynomial),
			KnConstant(64 - 4, Polynomial), KnConstant(64 + 4, Polynomial));

		for( ; Data.size() >= 64; Data = Data.subspan(64) )
		{

			const __m512i MulLo
				= _mm512_clmulepi64_epi128(CRCVec_512, K1K2, 0b0000'0000);

			const __m512i MulHi
				= _mm512_clmulepi64_epi128(CRCVec_512, K1K2, 0b0001'0001);

			const __m512i Load = _mm512_loadu_si512(
				&reinterpret_cast<const __m512i*>(Data.data())[0]);

			CRCVec_512 = _mm512_xor_si512(_mm512_xor_si512(MulHi, MulLo), Load);
		}

		CRCVec0 = _mm512_extracti32x4_epi32(CRCVec_512, 0);
		CRCVec1 = _mm512_extracti32x4_epi32(CRCVec_512, 1);
		CRCVec2 = _mm512_extracti32x4_epi32(CRCVec_512, 2);
		CRCVec3 = _mm512_extracti32x4_epi32(CRCVec_512, 3);
	}
#else
	for( ; Data.size() >= 64; Data = Data.subspan(64) )
	{
		const __m128i K1K2 = _mm_set_epi64x(
			KnConstant(64 - 4, Polynomial), KnConstant(64 + 4, Polynomial));

		const __m128i MulLo0 = _mm_clmulepi64_si128(CRCVec0, K1K2, 0b0000'0000);
		const __m128i MulLo1 = _mm_clmulepi64_si128(CRCVec1, K1K2, 0b0000'0000);
		const __m128i MulLo2 = _mm_clmulepi64_si128(CRCVec2, K1K2, 0b0000'0000);
		const __m128i MulLo3 = _mm_clmulepi64_si128(CRCVec3, K1K2, 0b0000'0000);

		const __m128i MulHi0 = _mm_clmulepi64_si128(CRCVec0, K1K2, 0b0001'0001);
		const __m128i MulHi1 = _mm_clmulepi64_si128(CRCVec1, K1K2, 0b0001'0001);
		const __m128i MulHi2 = _mm_clmulepi64_si128(CRCVec2, K1K2, 0b0001'0001);
		const __m128i MulHi3 = _mm_clmulepi64_si128(CRCVec3, K1K2, 0b0001'0001);

		const __m128i Load0 = _mm_loadu_si128(
			&reinterpret_cast<const __m128i*>(Data.data())[0]);
		const __m128i Load1 = _mm_loadu_si128(
			&reinterpret_cast<const __m128i*>(Data.data())[1]);
		const __m128i Load2 = _mm_loadu_si128(
			&reinterpret_cast<const __m128i*>(Data.data())[2]);
		const __m128i Load3 = _mm_loadu_si128(
			&reinterpret_cast<const __m128i*>(Data.data())[3]);

		CRCVec0 = _mm_xor_si128(_mm_xor_si128(MulHi0, MulLo0), Load0);
		CRCVec1 = _mm_xor_si128(_mm_xor_si128(MulHi1, MulLo1), Load1);
		CRCVec2 = _mm_xor_si128(_mm_xor_si128(MulHi2, MulLo2), Load2);
		CRCVec3 = _mm_xor_si128(_mm_xor_si128(MulHi3, MulLo3), Load3);
	}
#endif

	// Reduce 512 to 128
	const __m128i K3K4 = _mm_set_epi64x(
		KnConstant(16 - 4, Polynomial), KnConstant(16 + 4, Polynomial));

	// Reduce Vec1 into Vec0
	{
		const __m128i MulLo = _mm_clmulepi64_si128(CRCVec0, K3K4, 0b0000'0000);
		const __m128i MulHi = _mm_clmulepi64_si128(CRCVec0, K3K4, 0b0001'0001);
		CRCVec0 = _mm_xor_si128(_mm_xor_si128(MulHi, MulLo), CRCVec1);
	}

	// Reduce Vec2 into Vec0
	{
		const __m128i MulLo = _mm_clmulepi64_si128(CRCVec0, K3K4, 0b0000'0000);
		const __m128i MulHi = _mm_clmulepi64_si128(CRCVec0, K3K4, 0b0001'0001);
		CRCVec0 = _mm_xor_si128(_mm_xor_si128(MulHi, MulLo), CRCVec2);
	}

	// Reduce Vec3 into Vec0
	{
		const __m128i MulLo = _mm_clmulepi64_si128(CRCVec0, K3K4, 0b0000'0000);
		const __m128i MulHi = _mm_clmulepi64_si128(CRCVec0, K3K4, 0b0001'0001);
		CRCVec0 = _mm_xor_si128(_mm_xor_si128(MulHi, MulLo), CRCVec3);
	}

	// Fold 128 bits at a time
	for( ; Data.size() >= 16; Data = Data.subspan(16) )
	{
		const __m128i Load
			= _mm_loadu_si128(reinterpret_cast<const __m128i*>(Data.data()));

		const __m128i MulLo = _mm_clmulepi64_si128(CRCVec0, K3K4, 0b0000'0000);
		const __m128i MulHi = _mm_clmulepi64_si128(CRCVec0, K3K4, 0b0001'0001);

		CRCVec0 = _mm_xor_si128(_mm_xor_si128(MulHi, MulLo), Load);
	}

	// Reduce 128 to 64
	const __m128i Lo32Mask64 = _mm_set1_epi64x(0xFFFFFFFF);
	{
		const __m128i MulHiLo
			= _mm_clmulepi64_si128(CRCVec0, K3K4, 0b0001'0000);

		const __m128i Upper64 = _mm_srli_si128(CRCVec0, 8);

		CRCVec0 = _mm_xor_si128(Upper64, MulHiLo);

		const __m128i K5K0 = _mm_cvtsi64_si128(KnConstant(8, Polynomial));

		const __m128i Upper96 = _mm_srli_si128(CRCVec0, 4);

		const __m128i Trunc32 = _mm_and_si128(CRCVec0, Lo32Mask64);

		const __m128i MulLo = _mm_clmulepi64_si128(Trunc32, K5K0, 0b0000'0000);

		CRCVec0 = _mm_xor_si128(MulLo, Upper96);
	}

	// Reduce 64 to 32
	{
		const __m128i Poly = _mm_set_epi64x(
			MuConstant(Polynomial), KnConstant(4, Polynomial) | 1);

		__m128i Trunc32 = _mm_and_si128(CRCVec0, Lo32Mask64);

		const __m128i MulHiLo
			= _mm_clmulepi64_si128(Trunc32, Poly, 0b0001'0000);

		Trunc32             = _mm_and_si128(MulHiLo, Lo32Mask64);
		const __m128i MulLo = _mm_clmulepi64_si128(Trunc32, Poly, 0b0000'0000);

		CRCVec0 = _mm_xor_si128(CRCVec0, MulLo);
	}

	return _mm_extract_epi32(CRCVec0, 1);
}
#endif

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

std::uint32_t Checksum(
	std::span<const std::byte> Data, std::uint32_t InitialValue,
	Polynomial Poly)
{

	const auto&   Table = GetCRC32Table(Poly);
	std::uint32_t CRC   = ~InitialValue;

#if defined(__PCLMUL__)
	if( Poly == Polynomial::CRC32 )
	{
		if( Data.size() >= 64 )
		{
			if( Data.size() % 16 == 0 )
			{
				return ~CRC32_PCLMULQDQ<BitReverse32(
					std::uint32_t(Polynomial::CRC32))>(Data, CRC);
			}
			else
			{
				CRC = CRC32_PCLMULQDQ<BitReverse32(
					std::uint32_t(Polynomial::CRC32))>(Data, CRC);
				return Checksum(Data.last(Data.size() % 16), ~CRC);
			}
		}
	}
#endif

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
			(sizeof(CRC32TableT::value_type) / 4) * 0,
			(sizeof(CRC32TableT::value_type) / 4) * 1,
			(sizeof(CRC32TableT::value_type) / 4) * 2,
			(sizeof(CRC32TableT::value_type) / 4) * 3,
			(sizeof(CRC32TableT::value_type) / 4) * 4,
			(sizeof(CRC32TableT::value_type) / 4) * 5,
			(sizeof(CRC32TableT::value_type) / 4) * 6,
			(sizeof(CRC32TableT::value_type) / 4) * 7,
			(sizeof(CRC32TableT::value_type) / 4) * 8,
			(sizeof(CRC32TableT::value_type) / 4) * 9,
			(sizeof(CRC32TableT::value_type) / 4) * 10,
			(sizeof(CRC32TableT::value_type) / 4) * 11,
			(sizeof(CRC32TableT::value_type) / 4) * 12,
			(sizeof(CRC32TableT::value_type) / 4) * 13,
			(sizeof(CRC32TableT::value_type) / 4) * 14,
			(sizeof(CRC32TableT::value_type) / 4) * 15);
		for( ; Data.size() / 16; )
		{
			// Load in 8 bytes
			const std::uint64_t Input64Lo
				= *reinterpret_cast<const std::uint64_t*>(Data.data()) ^ CRC;
			Data = Data.subspan(sizeof(std::uint64_t));
			const std::uint64_t Input64Hi
				= *reinterpret_cast<const std::uint64_t*>(Data.data());
			Data = Data.subspan(sizeof(std::uint64_t));
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
		for( ; Data.size() / 16; )
		{
			const std::uint32_t InputLoLo
				= *reinterpret_cast<const std::uint32_t*>(Data.data()) ^ CRC;
			Data = Data.subspan(sizeof(std::uint32_t));

			const std::uint32_t InputHiLo
				= *reinterpret_cast<const std::uint32_t*>(Data.data());
			Data = Data.subspan(sizeof(std::uint32_t));

			const std::uint32_t InputLoHi
				= *reinterpret_cast<const std::uint32_t*>(Data.data());
			Data = Data.subspan(sizeof(std::uint32_t));

			const std::uint32_t InputHiHi
				= *reinterpret_cast<const std::uint32_t*>(Data.data());
			Data = Data.subspan(sizeof(std::uint32_t));

			CRC = Table[15][std::uint8_t(InputLoLo)]
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
			(sizeof(CRC32TableT::value_type) / 4) * 0,
			(sizeof(CRC32TableT::value_type) / 4) * 1,
			(sizeof(CRC32TableT::value_type) / 4) * 2,
			(sizeof(CRC32TableT::value_type) / 4) * 3,
			(sizeof(CRC32TableT::value_type) / 4) * 4,
			(sizeof(CRC32TableT::value_type) / 4) * 5,
			(sizeof(CRC32TableT::value_type) / 4) * 6,
			(sizeof(CRC32TableT::value_type) / 4) * 7);
		for( ; Data.size() / 8; Data.subspan(8) )
		{
			// Load in 8 bytes
			const std::uint64_t Input64
				= *reinterpret_cast<const std::uint64_t*>(Data.data()) ^ CRC;
			Data = Data.subspan(sizeof(std::uint64_t));
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
		for( ; Data.size() / 8; )
		{
			const std::uint32_t InputLo
				= *reinterpret_cast<const std::uint32_t*>(Data.data()) ^ CRC;
			Data = Data.subspan(sizeof(std::uint32_t));

			const std::uint32_t InputHi
				= *reinterpret_cast<const std::uint32_t*>(Data.data());
			Data = Data.subspan(sizeof(std::uint32_t));

			CRC = Table[7][std::uint8_t(InputLo)]
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
		Data.begin(), Data.end(), CRC,
		[Table](std::uint32_t CurCRC, std::byte Byte) -> std::uint32_t {
			return (CurCRC >> 8)
				 ^ Table[0][std::uint8_t(CurCRC) ^ std::uint8_t(Byte)];
		});
}

} // namespace CRC

#endif