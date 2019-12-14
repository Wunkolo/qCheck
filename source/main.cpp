#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <charconv>
#include <algorithm>
#include <numeric>

#include <vector>
#include <filesystem>
#include <fstream>
#include <iterator>

#include <getopt.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <fcntl.h>


#ifdef __AVX2__
#include <immintrin.h>
#endif

struct Settings
{
	std::filesystem::path ChecksumFile;
	std::vector<std::filesystem::path> InputFiles;
	std::size_t Threads            = 2;
	bool Verbose                   = true;
};

const char* Usage = 
"qCheck - Wunkolo <wunkolo@gmail.com>\n"
"Usage: qCheck [Options]... [Files]...\n"
"  -h, --help               Show this help message\n"
"  -t, --threads            Number of checker threads in parallel. Default: 2\n";

const static struct option CommandOptions[5] = {
	{ "threads",  optional_argument, nullptr,  't' },
	{ "help",     optional_argument, nullptr,  'h' },
	{ nullptr,          no_argument, nullptr, '\0' }
};

constexpr std::array<std::array<std::uint32_t, 256>, 8> CRC32Table(
	std::uint32_t Polynomial
) noexcept
{
	std::array<std::array<std::uint32_t, 256>, 8> Table = {};
	for( std::size_t i = 0; i < 256; ++i )
	{
		std::uint32_t CRC = i;
		for( std::size_t CurBit = 0; CurBit < 8; ++CurBit )
		{
			CRC = (CRC >> 1) ^ ( -(CRC & 0b1) & Polynomial);
		}
		Table[0][i] = CRC;
	}

	for( std::size_t i = 0; i < 256; ++i )
	{
		Table[1][i] = (Table[0][i] >> 8) ^ Table[0][std::uint8_t(Table[0][i])];
		Table[2][i] = (Table[1][i] >> 8) ^ Table[0][std::uint8_t(Table[1][i])];
		Table[3][i] = (Table[2][i] >> 8) ^ Table[0][std::uint8_t(Table[2][i])];
		Table[4][i] = (Table[3][i] >> 8) ^ Table[0][std::uint8_t(Table[3][i])];
		Table[5][i] = (Table[4][i] >> 8) ^ Table[0][std::uint8_t(Table[4][i])];
		Table[6][i] = (Table[5][i] >> 8) ^ Table[0][std::uint8_t(Table[5][i])];
		Table[7][i] = (Table[6][i] >> 8) ^ Table[0][std::uint8_t(Table[6][i])];
	}

	return Table;
}

#ifdef __AVX2__
inline std::uint32_t _mm256_hxor_epi32(__m256i a)
{
	// Xor top half with bottom half
	const __m128i XorReduce128 = _mm_xor_si128(
		_mm256_extracti128_si256(a, 1),
		_mm256_extracti128_si256(a, 0)
	);
	const std::uint64_t XorReduce64 =
		_mm_extract_epi64(XorReduce128, 1) ^
		_mm_extract_epi64(XorReduce128, 0);
	return XorReduce64 ^ (XorReduce64 >> 32);
}
#endif

// Todo: This should technically be "contiguous_iterator_tag" from C++20
template< typename RandomIterator, std::uint32_t Polynomial >
std::uint32_t Checksum(RandomIterator First, RandomIterator Last, std::random_access_iterator_tag)
{
	static constexpr auto Table = CRC32Table(Polynomial);
	std::uint32_t CRC = ~0;
	const std::size_t Size = static_cast<std::size_t>(std::distance(First, Last));

	const std::uint32_t* Input32 = reinterpret_cast<const std::uint32_t*>(&(*First));
	std::size_t i;

	// Slice by 8
#ifdef __AVX2__
	for( i = 0; i < Size / 8; ++i )
	{
		const std::uint64_t Input64 =
			*reinterpret_cast<const std::uint64_t*>(Input32) ^ CRC;
		Input32 += 2;
		const __m256i Indices = _mm256_shuffle_epi8(
			_mm256_set1_epi64x(Input64),
			_mm256_set_epi8(
				~0, ~0, ~0, 7,
				~0, ~0, ~0, 6,
				~0, ~0, ~0, 5,
				~0, ~0, ~0, 4,
				~0, ~0, ~0, 3,
				~0, ~0, ~0, 2,
				~0, ~0, ~0, 1,
				~0, ~0, ~0, 0
			)
		);
		const __m256i ArrayOffset = _mm256_set_epi32(
			(sizeof(typename decltype(Table)::value_type) / 4) * 0,
			(sizeof(typename decltype(Table)::value_type) / 4) * 1,
			(sizeof(typename decltype(Table)::value_type) / 4) * 2,
			(sizeof(typename decltype(Table)::value_type) / 4) * 3,
			(sizeof(typename decltype(Table)::value_type) / 4) * 4,
			(sizeof(typename decltype(Table)::value_type) / 4) * 5,
			(sizeof(typename decltype(Table)::value_type) / 4) * 6,
			(sizeof(typename decltype(Table)::value_type) / 4) * 7 
		);
		const __m256i Gather = _mm256_i32gather_epi32(
			reinterpret_cast<const std::int32_t*>(Table.data()),
			_mm256_add_epi32(Indices, ArrayOffset), sizeof(std::uint32_t)
		);
		CRC = _mm256_hxor_epi32(Gather);
	}
#else
	for( i = 0; i < Size / 8; ++i )
	{
		const std::uint32_t InputLo = *Input32++ ^ CRC;
		const std::uint32_t InputHi = *Input32++;
		CRC =
			Table[7][std::uint8_t(InputLo      )] ^
			Table[6][std::uint8_t(InputLo >>  8)] ^
			Table[5][std::uint8_t(InputLo >> 16)] ^
			Table[4][std::uint8_t(InputLo >> 24)] ^
			Table[3][std::uint8_t(InputHi      )] ^
			Table[2][std::uint8_t(InputHi >>  8)] ^
			Table[1][std::uint8_t(InputHi >> 16)] ^
			Table[0][std::uint8_t(InputHi >> 24)];
	}
#endif

	First += (i * 8);
	return ~std::accumulate(
		First, Last, CRC,
		[](std::uint32_t CRC, std::uint8_t Byte) 
		{
			return (CRC >> 8) ^ Table[0][std::uint8_t(CRC) ^ Byte];
		}
	);
}

template< typename Iterator, std::uint32_t Polynomial >
std::uint32_t Checksum(Iterator First, Iterator Last, std::input_iterator_tag)
{
	static constexpr auto Table = CRC32Table(Polynomial);
	return ~std::accumulate(
		First, Last, ~std::uint32_t(0),
		[](std::uint32_t CRC, std::uint8_t Byte) 
		{
			return (CRC >> 8) ^ Table[0][std::uint8_t(CRC) ^ Byte];
		}
	);
}

template< std::uint32_t Polynomial, typename Iterator>
inline std::uint32_t Checksum(Iterator First, Iterator Last)
{
	return Checksum<Iterator, Polynomial>(
		First, Last,
		typename std::iterator_traits<Iterator>::iterator_category()
	);
}

int main( int argc, char* argv[] )
{
	Settings CurSettings = {};
	int Opt;
	int OptionIndex;
	// Parse Arguments
	while( (Opt = getopt_long(argc, argv, "t:h", CommandOptions, &OptionIndex )) != -1 )
	{
		switch( Opt )
		{
		case 't':
		{
			std::size_t Threads;
			const auto ParseResult = std::from_chars<std::size_t>(
				optarg, optarg + std::strlen(optarg),
				Threads
			);
			if(
				*ParseResult.ptr != '\0'
				|| ParseResult.ec != std::errc()
			)
			{
				std::fprintf(
					stdout,
					"Invalid thread count \"%s\"\n",
					optarg
				);
				return EXIT_FAILURE;
			}
			CurSettings.Threads = static_cast<std::size_t>(Threads);
			break;
		}
		case 'h':
		{
			std::puts(Usage);
			return EXIT_SUCCESS;
		}
		default:
		{
			std::puts(Usage);
			return EXIT_FAILURE;
		}
		}
	}
	argc -= optind;
	argv += optind;

	// Check for config errors here

	// Parse file list
	std::fprintf(
		stdout,
		"; Generated with qCheck by Wunkolo [ Build: " __TIMESTAMP__" ]\n"
	);

	for( std::intmax_t i = 0; i < argc; ++i )
	{
		const std::filesystem::path CurPath(argv[i]);
		if( !std::filesystem::exists(CurPath) )
		{
			std::fprintf(stderr, "File does not exist: %s\n", argv[i]);
			continue;
		}
		std::error_code CurError;
		std::size_t FileSize = 0;
		if( std::filesystem::is_regular_file(CurPath, CurError) )
		{
			FileSize = std::filesystem::file_size(CurPath);
		}
		CurSettings.InputFiles.emplace_back(CurPath);
		std::fprintf(
			stdout, "; %zu %s\n",
			FileSize,
			CurPath.filename().c_str()
		);
	}

	for( const auto& CurPath : CurSettings.InputFiles )
	{
		std::uint32_t CRC32 = 0;
		std::error_code CurError;
		if( std::filesystem::is_regular_file(CurPath, CurError) )
		{
			// Regular file sitting on some storage media, unchanging
			// Use a faster mmap path.
			const auto FileSize = std::filesystem::file_size(CurPath);
			const auto FileHandle = open(CurPath.c_str(), O_RDONLY, 0);
			void* FileMap = mmap(
				nullptr, FileSize,
				PROT_READ, MAP_SHARED | MAP_POPULATE,
				FileHandle, 0
			);
			madvise(FileMap, FileSize, MADV_SEQUENTIAL | MADV_WILLNEED);
			CRC32 = Checksum<0xEDB88320u, const std::uint8_t*>(
				reinterpret_cast<const std::uint8_t*>(FileMap),
				reinterpret_cast<const std::uint8_t*>(FileMap) + FileSize
			);
			munmap((void*)FileMap, FileSize);
			close(FileHandle);
		}
		else
		{
			std::ifstream CurFile(CurPath, std::ios::binary);
			CRC32 = Checksum<0xEDB88320u>(
				std::istreambuf_iterator<char>(CurFile),
				std::istreambuf_iterator<char>()
			);
		}
		std::fprintf(
			stdout, "%s %08X\n",
			CurPath.filename().c_str(),
			CRC32
		);
	}

	return EXIT_SUCCESS;
}
