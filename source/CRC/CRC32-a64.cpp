#if defined(__aarch64__)

#include <CRC32.hpp>

#include <arm_neon.h>

#if !defined(ARMV8_OS_MACOS)
#define __crc32d __builtin_arm_crc32d
#define __crc32w __builtin_arm_crc32w
#define __crc32h __builtin_arm_crc32h
#define __crc32b __builtin_arm_crc32b
#define __crc32cd __builtin_arm_crc32cd
#define __crc32cw __builtin_arm_crc32cw
#define __crc32ch __builtin_arm_crc32ch
#define __crc32cb __builtin_arm_crc32cb
#endif

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

constexpr std::uint32_t BitReverse32(std::uint32_t Value)
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
constexpr std::uint64_t
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
constexpr std::uint64_t MuConstant(uint32_t Polynomial)
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

// Simulates the behavior of the `_mm_clmulepi64_si128` instruction found on x64
// Ex:
//  pmull_p64<0,0> = _mm_clmulepi64_si128(..., ..., 0b0000'0000)
//  pmull_p64<0,1> = _mm_clmulepi64_si128(..., ..., 0b0000'0001)
//  pmull_p64<1,0> = _mm_clmulepi64_si128(..., ..., 0b0001'0000)
//  pmull_p64<1,1> = _mm_clmulepi64_si128(..., ..., 0b0001'0001)
template<std::size_t LaneB, std::size_t LaneA>
inline poly64x2_t pmull_p64(const poly64x2_t OpA, const poly64x2_t OpB)
{
	return (poly64x2_t)vmull_p64(
		vgetq_lane_u64(OpA, LaneA), vgetq_lane_u64(OpB, LaneB));
}

template<>
inline poly64x2_t pmull_p64<1, 1>(const poly64x2_t OpA, const poly64x2_t OpB)
{
	return (poly64x2_t)vmull_high_p64(OpA, OpB);
}

inline poly64x2_t
	eor3_p64(const poly64x2_t OpA, const poly64x2_t OpB, const poly64x2_t OpC)
{
#if defined(__ARM_FEATURE_SHA3)
	return veor3q_u64(OpA, OpB, OpC);
#else
	return veorq_u64(OpA, veorq_u64(OpB, OpC));
#endif
}

template<std::uint32_t Polynomial>
std::uint32_t CRC32_PMULL(std::span<const std::byte> Data, std::uint32_t CRC)
{
	poly64x2x4_t CRCVec
		= vld1q_p64_x4(reinterpret_cast<const poly64_t*>(Data.data()));

	Data = Data.subspan(64);

	CRCVec.val[0]
		= veorq_u64(CRCVec.val[0], vsetq_lane_s32(CRC, vdupq_n_s32(0), 0));

	// Fold 512 bits at a time
	for( ; Data.size() >= 64; Data = Data.subspan(64) )
	{
		static const poly64x2_t K1K2 = poly64x2_t{
			KnConstant(64 + 4, Polynomial),
			KnConstant(64 - 4, Polynomial),
		};

		const poly64x2_t MulLo0 = pmull_p64<0, 0>(CRCVec.val[0], K1K2);
		const poly64x2_t MulLo1 = pmull_p64<0, 0>(CRCVec.val[1], K1K2);
		const poly64x2_t MulLo2 = pmull_p64<0, 0>(CRCVec.val[2], K1K2);
		const poly64x2_t MulLo3 = pmull_p64<0, 0>(CRCVec.val[3], K1K2);

		const poly64x2_t MulHi0 = pmull_p64<1, 1>(CRCVec.val[0], K1K2);
		const poly64x2_t MulHi1 = pmull_p64<1, 1>(CRCVec.val[1], K1K2);
		const poly64x2_t MulHi2 = pmull_p64<1, 1>(CRCVec.val[2], K1K2);
		const poly64x2_t MulHi3 = pmull_p64<1, 1>(CRCVec.val[3], K1K2);

		const poly64x2x4_t Load
			= vld1q_p64_x4(reinterpret_cast<const poly64_t*>(Data.data()));

		CRCVec.val[0] = eor3_p64(MulHi0, MulLo0, Load.val[0]);
		CRCVec.val[1] = eor3_p64(MulHi1, MulLo1, Load.val[1]);
		CRCVec.val[2] = eor3_p64(MulHi2, MulLo2, Load.val[2]);
		CRCVec.val[3] = eor3_p64(MulHi3, MulLo3, Load.val[3]);
	}

	// Reduce 512 to 128
	static const poly64x2_t K3K4 = poly64x2_t{
		KnConstant(16 + 4, Polynomial),
		KnConstant(16 - 4, Polynomial),
	};

	// Reduce Vec1 into Vec0
	{
		const poly64x2_t MulLo = pmull_p64<0, 0>(CRCVec.val[0], K3K4);
		const poly64x2_t MulHi = pmull_p64<1, 1>(CRCVec.val[0], K3K4);
		CRCVec.val[0]          = eor3_p64(MulHi, MulLo, CRCVec.val[1]);
	}

	// Reduce Vec2 into Vec0
	{
		const poly64x2_t MulLo = pmull_p64<0, 0>(CRCVec.val[0], K3K4);
		const poly64x2_t MulHi = pmull_p64<1, 1>(CRCVec.val[0], K3K4);
		CRCVec.val[0]          = eor3_p64(MulHi, MulLo, CRCVec.val[2]);
	}

	// Reduce Vec3 into Vec0
	{
		const poly64x2_t MulLo = pmull_p64<0, 0>(CRCVec.val[0], K3K4);
		const poly64x2_t MulHi = pmull_p64<1, 1>(CRCVec.val[0], K3K4);
		CRCVec.val[0]          = eor3_p64(MulHi, MulLo, CRCVec.val[3]);
	}

	// Fold 128 bits at a time
	for( ; Data.size() >= 16; Data = Data.subspan(16) )
	{
		const poly64x2_t Load
			= vld1q_p64(reinterpret_cast<const poly64_t*>(Data.data()));

		const poly64x2_t MulLo = pmull_p64<0, 0>(CRCVec.val[0], K3K4);
		const poly64x2_t MulHi = pmull_p64<1, 1>(CRCVec.val[0], K3K4);
		CRCVec.val[0]          = eor3_p64(MulHi, MulLo, Load);
	}

	// Reduce 128 to 64
	static const poly64x2_t Lo32Mask64 = vdupq_n_p64(0xFFFFFFFF);
	{
		const poly64x2_t MulHiLo = pmull_p64<1, 0>(CRCVec.val[0], K3K4);

		const poly64x2_t Upper64 = vextq_s8(CRCVec.val[0], vdupq_n_s8(0), 8);

		CRCVec.val[0] = veorq_u64(Upper64, MulHiLo);

		static const poly64x2_t K5K0 = poly64x2_t{
			KnConstant(8, Polynomial),
			0ull,
		};

		const poly64x2_t Upper96 = vextq_s8(CRCVec.val[0], vdupq_n_s8(0), 4);

		const poly64x2_t Trunc32 = vandq_u64(CRCVec.val[0], Lo32Mask64);

		const poly64x2_t MulLo = pmull_p64<0, 0>(Trunc32, K5K0);

		CRCVec.val[0] = veorq_u64(MulLo, Upper96);
	}

	// Reduce 64 to 32
	{
		static const poly64x2_t Poly = poly64x2_t{
			KnConstant(4, Polynomial) | 1,
			MuConstant(Polynomial),
		};

		poly64x2_t Trunc32 = vandq_u64(CRCVec.val[0], Lo32Mask64);

		const poly64x2_t MulHiLo = pmull_p64<1, 0>(Trunc32, Poly);

		Trunc32                = vandq_u64(MulHiLo, Lo32Mask64);
		const poly64x2_t MulLo = pmull_p64<0, 0>(Trunc32, Poly);

		CRCVec.val[0] = veorq_u64(CRCVec.val[0], MulLo);
	}

	return vgetq_lane_u32(CRCVec.val[0], 1);
}

std::uint32_t Checksum(
	std::span<const std::byte> Data, std::uint32_t InitialValue,
	Polynomial Poly)
{

	const auto&   Table = GetCRC32Table(Poly);
	std::uint32_t CRC   = ~InitialValue;

	if( Poly == Polynomial::CRC32 )
	{
		const std::uint32_t Polynomial32 = std::uint32_t(Poly);
		if( Data.size() >= 64 )
		{
			if( Data.size() % 16 == 0 )
			{
				return ~CRC32_PMULL<BitReverse32(
					std::uint32_t(Polynomial::CRC32))>(Data, CRC);
			}
			else
			{
				CRC = CRC32_PMULL<BitReverse32(
					std::uint32_t(Polynomial::CRC32))>(Data, CRC);
				return Checksum(Data.last(Data.size() % 16), ~CRC);
			}
		}
	}

#if defined(__ARM_FEATURE_CRC32)
	if( Poly == Polynomial::CRC32 )
	{
		for( ; Data.size() / 8; )
		{
			const std::uint64_t Input64
				= *reinterpret_cast<const std::uint64_t*>(Data.data());

			CRC  = __crc32d(CRC, Input64);
			Data = Data.subspan(sizeof(std::uint64_t));
		}
		for( ; Data.size() / 4; )
		{
			const std::uint32_t Input32
				= *reinterpret_cast<const std::uint32_t*>(Data.data());

			CRC  = __crc32w(CRC, Input32);
			Data = Data.subspan(sizeof(std::uint32_t));
		}
		for( ; Data.size() / 2; )
		{
			const std::uint16_t Input16
				= *reinterpret_cast<const std::uint16_t*>(Data.data());

			CRC  = __crc32h(CRC, Input16);
			Data = Data.subspan(sizeof(std::uint16_t));
		}
		for( ; Data.size(); )
		{
			const std::uint8_t Input8
				= *reinterpret_cast<const std::uint8_t*>(Data.data());

			CRC  = __crc32b(CRC, Input8);
			Data = Data.subspan(sizeof(std::uint8_t));
		}
	}
	else if( Poly == Polynomial::CRC32C )
	{
		for( ; Data.size() / 8; )
		{
			const std::uint64_t Input64
				= *reinterpret_cast<const std::uint64_t*>(Data.data());

			CRC  = __crc32cd(CRC, Input64);
			Data = Data.subspan(sizeof(std::uint64_t));
		}
		for( ; Data.size() / 4; )
		{
			const std::uint32_t Input32
				= *reinterpret_cast<const std::uint32_t*>(Data.data());

			CRC  = __crc32cw(CRC, Input32);
			Data = Data.subspan(sizeof(std::uint32_t));
		}
		for( ; Data.size() / 2; )
		{
			const std::uint16_t Input16
				= *reinterpret_cast<const std::uint16_t*>(Data.data());

			CRC  = __crc32ch(CRC, Input16);
			Data = Data.subspan(sizeof(std::uint16_t));
		}
		for( ; Data.size(); )
		{
			const std::uint8_t Input8
				= *reinterpret_cast<const std::uint8_t*>(Data.data());

			CRC  = __crc32cb(CRC, Input8);
			Data = Data.subspan(sizeof(std::uint8_t));
		}
	}
#endif

	// Slice by 16
	{
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
	}

	// Slice by 8
	{
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