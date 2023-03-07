#include <CRC/CRC32.hpp>

#include <array>
#include <random>
#include <span>
#include <string_view>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("Null bytes", "[CRC32]")
{
	std::array<std::uint8_t, 0> Data;

	const std::uint32_t Checksum
		= CRC::Checksum(std::as_bytes(std::span{Data}));
	REQUIRE(Checksum == 0x00000000);
}

TEST_CASE("IOTAx8 (byte)", "[CRC32]")
{
	std::array<std::uint8_t, 8> Data;
	std::iota(Data.begin(), Data.end(), 0);

	const std::uint32_t Checksum
		= CRC::Checksum(std::as_bytes(std::span{Data}));
	REQUIRE(Checksum == 0x88AA689F);
}

TEST_CASE("IOTAx16 (byte)", "[CRC32]")
{
	std::array<std::uint8_t, 16> Data;
	std::iota(Data.begin(), Data.end(), 0);

	const std::uint32_t Checksum
		= CRC::Checksum(std::as_bytes(std::span{Data}));
	REQUIRE(Checksum == 0xCECEE288);
}

TEST_CASE("IOTAx32 (byte)", "[CRC32]")
{
	std::array<std::uint8_t, 32> Data;
	std::iota(Data.begin(), Data.end(), 0);

	const std::uint32_t Checksum
		= CRC::Checksum(std::as_bytes(std::span{Data}));
	REQUIRE(Checksum == 0x91267E8A);
}

TEST_CASE("IOTAx64 (byte)", "[CRC32]")
{
	std::array<std::uint8_t, 64> Data;
	std::iota(Data.begin(), Data.end(), 0);

	const std::uint32_t Checksum
		= CRC::Checksum(std::as_bytes(std::span{Data}));
	REQUIRE(Checksum == 0x100ECE8C);
}

TEST_CASE("IOTAx128 (byte)", "[CRC32]")
{
	std::array<std::uint8_t, 128> Data;
	std::iota(Data.begin(), Data.end(), 0);

	const std::uint32_t Checksum
		= CRC::Checksum(std::as_bytes(std::span{Data}));
	REQUIRE(Checksum == 0x24650D57);
}

TEST_CASE("IOTAx256 (byte)", "[CRC32]")
{
	std::array<std::uint8_t, 256> Data;
	std::iota(Data.begin(), Data.end(), 0);

	const std::uint32_t Checksum
		= CRC::Checksum(std::as_bytes(std::span{Data}));
	REQUIRE(Checksum == 0x29058C73);
}

TEST_CASE("IOTAx512 (byte)", "[CRC32]")
{
	std::array<std::uint8_t, 512> Data;
	std::iota(Data.begin(), Data.end(), 0);

	const std::uint32_t Checksum
		= CRC::Checksum(std::as_bytes(std::span{Data}));
	REQUIRE(Checksum == 0x1C613576);
}

TEST_CASE("IOTAx1024 (byte)", "[CRC32]")
{
	std::array<std::uint8_t, 1024> Data;
	std::iota(Data.begin(), Data.end(), 0);

	const std::uint32_t Checksum
		= CRC::Checksum(std::as_bytes(std::span{Data}));
	REQUIRE(Checksum == 0xB70B4C26);
}

TEST_CASE("\'penguin\'", "[CRC32]")
{
	const char String[] = "penguin";
	const auto Data     = std::string_view(String);

	const std::uint32_t Checksum
		= CRC::Checksum(std::as_bytes(std::span{Data}));
	REQUIRE(Checksum == 0x0E5C1A120);
}

TEST_CASE("mt19937_32x1024", "[CRC32]")
{
	std::mt19937 MersenneTwister;

	std::array<std::uint32_t, 1024> Data = {};
	for( auto& CurValue : Data )
	{
		CurValue = MersenneTwister();
	}

	const std::uint32_t Checksum
		= CRC::Checksum(std::as_bytes(std::span{Data}));
	REQUIRE(Checksum == 0x25396D17);
}

// This seems to produce different results between debug/release...
// Tue Oct  4 06:45:55 PM PDT 2022
// TEST_CASE("mt19937_64x1024", "[CRC32]")
// {
// 	std::mt19937_64 MersenneTwister;

// 	std::array<std::uint64_t, 1024> Data = {};
// 	for( auto& CurValue : Data )
// 	{
// 		CurValue = MersenneTwister();
// 	}

// 	const std::uint32_t Checksum
// 		= CRC::Checksum(std::as_bytes(std::span{Data}));
// 	REQUIRE(Checksum == 0x46BD489B);
// }

TEST_CASE("mt19937_32x797 (byte)", "[CRC32]")
{
	std::mt19937 MersenneTwister;

	std::array<std::uint8_t, 797> Data = {};
	for( auto& CurValue : Data )
	{
		CurValue = MersenneTwister();
	}

	const std::uint32_t Checksum
		= CRC::Checksum(std::as_bytes(std::span{Data}));
	REQUIRE(Checksum == 0xB4B21120);
}

TEST_CASE("mt19937_32x997 (byte)", "[CRC32]")
{
	std::mt19937 MersenneTwister;

	std::array<std::uint8_t, 997> Data = {};
	for( auto& CurValue : Data )
	{
		CurValue = MersenneTwister();
	}

	const std::uint32_t Checksum
		= CRC::Checksum(std::as_bytes(std::span{Data}));
	REQUIRE(Checksum == 0x3D27DCE0);
}

TEST_CASE("mt19937_32x1024x2 Combine", "[CRC32]")
{
	std::mt19937 MersenneTwister;

	std::array<std::uint32_t, 1024> DataA = {};
	for( auto& CurValue : DataA )
	{
		CurValue = MersenneTwister();
	}

	std::array<std::uint32_t, 1024> DataB = {};
	for( auto& CurValue : DataB )
	{
		CurValue = MersenneTwister();
	}

	MersenneTwister.seed(std::mt19937::default_seed);
	std::array<std::uint32_t, 2048> DataAB = {};
	for( auto& CurValue : DataAB )
	{
		CurValue = MersenneTwister();
	}

	const std::uint32_t ChecksumA
		= CRC::Checksum(std::as_bytes(std::span{DataA}));
	REQUIRE(ChecksumA == 0x25396D17);

	const std::uint32_t ChecksumB
		= CRC::Checksum(std::as_bytes(std::span{DataB}));
	REQUIRE(ChecksumB == 0x2FBB546D);

	const std::uint32_t ChecksumAB
		= CRC::Checksum(std::as_bytes(std::span{DataAB}));
	REQUIRE(ChecksumAB == 0x2E0FE81B);

	const std::uint32_t ChecksumABCombine
		= CRC::Checksum(std::as_bytes(std::span{DataB}), ChecksumA);
	REQUIRE(ChecksumABCombine == 0x2E0FE81B);
}