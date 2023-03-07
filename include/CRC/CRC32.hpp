#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <numeric>
#include <span>

#ifdef __x86_64__
#include <x86intrin.h>
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

std::uint32_t Checksum(
	std::span<const std::byte> Data, std::uint32_t InitialValue = 0u,
	Polynomial Poly = Polynomial::CRC32);

} // namespace CRC