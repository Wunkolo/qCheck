#include "CRC32.hpp"

#include <array>
#include <span>
#include <string_view>

#include <catch2/catch.hpp>

TEST_CASE("CRC: Null bytes")
{
	std::array<std::uint8_t, 0> Data;

	const std::uint32_t Checksum
		= CRC::Checksum<0xEDB88320u>(std::as_bytes(std::span{Data}));
	REQUIRE(Checksum == 0x0);
}

TEST_CASE("CRC: IOTAx8 (byte)")
{
	std::array<std::uint8_t, 8> Data;
	std::iota(Data.begin(), Data.end(), 0);

	const std::uint32_t Checksum
		= CRC::Checksum<0xEDB88320u>(std::as_bytes(std::span{Data}));
	REQUIRE(Checksum == 0x88aa689f);
}

TEST_CASE("CRC: IOTAx16 (byte)")
{
	std::array<std::uint8_t, 16> Data;
	std::iota(Data.begin(), Data.end(), 0);

	const std::uint32_t Checksum
		= CRC::Checksum<0xEDB88320u>(std::as_bytes(std::span{Data}));
	REQUIRE(Checksum == 0xcecee288);
}

TEST_CASE("CRC: IOTAx32 (byte)")
{
	std::array<std::uint8_t, 32> Data;
	std::iota(Data.begin(), Data.end(), 0);

	const std::uint32_t Checksum
		= CRC::Checksum<0xEDB88320u>(std::as_bytes(std::span{Data}));
	REQUIRE(Checksum == 0x91267e8a);
}

TEST_CASE("CRC: IOTAx64 (byte)")
{
	std::array<std::uint8_t, 64> Data;
	std::iota(Data.begin(), Data.end(), 0);

	const std::uint32_t Checksum
		= CRC::Checksum<0xEDB88320u>(std::as_bytes(std::span{Data}));
	REQUIRE(Checksum == 0x100ece8c);
}

TEST_CASE("CRC: IOTAx128 (byte)")
{
	std::array<std::uint8_t, 128> Data;
	std::iota(Data.begin(), Data.end(), 0);

	const std::uint32_t Checksum
		= CRC::Checksum<0xEDB88320u>(std::as_bytes(std::span{Data}));
	REQUIRE(Checksum == 0x24650d57);
}

TEST_CASE("CRC: IOTAx256 (byte)")
{
	std::array<std::uint8_t, 256> Data;
	std::iota(Data.begin(), Data.end(), 0);

	const std::uint32_t Checksum
		= CRC::Checksum<0xEDB88320u>(std::as_bytes(std::span{Data}));
	REQUIRE(Checksum == 0x29058c73);
}

TEST_CASE("CRC: IOTAx512 (byte)")
{
	std::array<std::uint8_t, 512> Data;
	std::iota(Data.begin(), Data.end(), 0);

	const std::uint32_t Checksum
		= CRC::Checksum<0xEDB88320u>(std::as_bytes(std::span{Data}));
	REQUIRE(Checksum == 0x1c613576);
}

TEST_CASE("CRC: IOTAx1024 (byte)")
{
	std::array<std::uint8_t, 1024> Data;
	std::iota(Data.begin(), Data.end(), 0);

	const std::uint32_t Checksum
		= CRC::Checksum<0xEDB88320u>(std::as_bytes(std::span{Data}));
	REQUIRE(Checksum == 0xb70b4c26);
}

TEST_CASE("CRC: \'penguin\'")
{
	const char String[] = "penguin";
	const auto Data     = std::string_view(String);

	const std::uint32_t Checksum
		= CRC::Checksum<0xEDB88320u>(std::as_bytes(std::span{Data}));
	REQUIRE(Checksum == 0x0e5c1a120);
}