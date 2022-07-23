#include "CRC32.hpp"

#include <array>
#include <random>
#include <span>
#include <string_view>

#include <catch2/catch.hpp>

TEST_CASE("Null bytes", "[CRC32]")
{
	std::array<std::uint8_t, 0> Data;

	const std::uint32_t Checksum
		= CRC::Checksum<0xEDB88320u>(std::as_bytes(std::span{Data}));
	REQUIRE(Checksum == 0x00000000);
}

TEST_CASE("IOTAx8 (byte)", "[CRC32]")
{
	std::array<std::uint8_t, 8> Data;
	std::iota(Data.begin(), Data.end(), 0);

	const std::uint32_t Checksum
		= CRC::Checksum<0xEDB88320u>(std::as_bytes(std::span{Data}));
	REQUIRE(Checksum == 0x88AA689F);
}

TEST_CASE("IOTAx16 (byte)", "[CRC32]")
{
	std::array<std::uint8_t, 16> Data;
	std::iota(Data.begin(), Data.end(), 0);

	const std::uint32_t Checksum
		= CRC::Checksum<0xEDB88320u>(std::as_bytes(std::span{Data}));
	REQUIRE(Checksum == 0xCECEE288);
}

TEST_CASE("IOTAx32 (byte)", "[CRC32]")
{
	std::array<std::uint8_t, 32> Data;
	std::iota(Data.begin(), Data.end(), 0);

	const std::uint32_t Checksum
		= CRC::Checksum<0xEDB88320u>(std::as_bytes(std::span{Data}));
	REQUIRE(Checksum == 0x91267E8A);
}

TEST_CASE("IOTAx64 (byte)", "[CRC32]")
{
	std::array<std::uint8_t, 64> Data;
	std::iota(Data.begin(), Data.end(), 0);

	const std::uint32_t Checksum
		= CRC::Checksum<0xEDB88320u>(std::as_bytes(std::span{Data}));
	REQUIRE(Checksum == 0x100ECE8C);
}

TEST_CASE("IOTAx128 (byte)", "[CRC32]")
{
	std::array<std::uint8_t, 128> Data;
	std::iota(Data.begin(), Data.end(), 0);

	const std::uint32_t Checksum
		= CRC::Checksum<0xEDB88320u>(std::as_bytes(std::span{Data}));
	REQUIRE(Checksum == 0x24650D57);
}

TEST_CASE("IOTAx256 (byte)", "[CRC32]")
{
	std::array<std::uint8_t, 256> Data;
	std::iota(Data.begin(), Data.end(), 0);

	const std::uint32_t Checksum
		= CRC::Checksum<0xEDB88320u>(std::as_bytes(std::span{Data}));
	REQUIRE(Checksum == 0x29058C73);
}

TEST_CASE("IOTAx512 (byte)", "[CRC32]")
{
	std::array<std::uint8_t, 512> Data;
	std::iota(Data.begin(), Data.end(), 0);

	const std::uint32_t Checksum
		= CRC::Checksum<0xEDB88320u>(std::as_bytes(std::span{Data}));
	REQUIRE(Checksum == 0x1C613576);
}

TEST_CASE("IOTAx1024 (byte)", "[CRC32]")
{
	std::array<std::uint8_t, 1024> Data;
	std::iota(Data.begin(), Data.end(), 0);

	const std::uint32_t Checksum
		= CRC::Checksum<0xEDB88320u>(std::as_bytes(std::span{Data}));
	REQUIRE(Checksum == 0xB70B4C26);
}

TEST_CASE("\'penguin\'", "[CRC32]")
{
	const char String[] = "penguin";
	const auto Data     = std::string_view(String);

	const std::uint32_t Checksum
		= CRC::Checksum<0xEDB88320u>(std::as_bytes(std::span{Data}));
	REQUIRE(Checksum == 0x0E5C1A120);
}

TEST_CASE("mt19937_32x1024", "[CRC32]")
{
	std::mt19937 MersenneTwister;
	MersenneTwister.seed(0);

	std::array<std::uint32_t, 1024> Data = {};
	for( auto& CurValue : Data )
	{
		CurValue = MersenneTwister();
	}

	const std::uint32_t Checksum
		= CRC::Checksum<0xEDB88320u>(std::as_bytes(std::span{Data}));
	REQUIRE(Checksum == 0xE3F613F7);
}

TEST_CASE("mt19937_64x1024", "[CRC32]")
{
	std::mt19937_64 MersenneTwister;
	MersenneTwister.seed(0);

	std::array<std::uint64_t, 1024> Data = {};
	for( auto& CurValue : Data )
	{
		CurValue = MersenneTwister();
	}

	const std::uint32_t Checksum
		= CRC::Checksum<0xEDB88320u>(std::as_bytes(std::span{Data}));
	REQUIRE(Checksum == 0X4AAEC4DF);
}

TEST_CASE("mt19937_32x797 (byte)", "[CRC32]")
{
	std::mt19937 MersenneTwister;
	MersenneTwister.seed(0);

	std::array<std::uint8_t, 797> Data = {};
	for( auto& CurValue : Data )
	{
		CurValue = MersenneTwister();
	}

	const std::uint32_t Checksum
		= CRC::Checksum<0xEDB88320u>(std::as_bytes(std::span{Data}));
	REQUIRE(Checksum == 0x74A9B4F2);
}

TEST_CASE("mt19937_32x997 (byte)", "[CRC32]")
{
	std::mt19937 MersenneTwister;
	MersenneTwister.seed(0);

	std::array<std::uint8_t, 997> Data = {};
	for( auto& CurValue : Data )
	{
		CurValue = MersenneTwister();
	}

	const std::uint32_t Checksum
		= CRC::Checksum<0xEDB88320u>(std::as_bytes(std::span{Data}));
	REQUIRE(Checksum == 0x83041FE4);
}