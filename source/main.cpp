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

constexpr std::array<std::array<std::uint32_t, 256>, 4> CRC32Table(
	std::uint32_t Polynomial
) noexcept
{
	std::array<std::array<std::uint32_t, 256>, 4> Table = {};
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
	}

	return Table;
}


template< typename RandomIterator, std::uint32_t Polynomial >
std::uint32_t Checksum(RandomIterator First, RandomIterator Last, std::random_access_iterator_tag)
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

template< typename RandomIterator, std::uint32_t Polynomial >
std::uint32_t Checksum(RandomIterator First, RandomIterator Last, std::input_iterator_tag)
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

template< typename Iterator >
inline std::uint32_t Checksum(Iterator First, Iterator Last)
{
	return Checksum<Iterator, 0xEDB88320u>(
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

	// Parse files
	for( std::size_t i = 0; i < argc; ++i )
	{
		const std::filesystem::path CurPath(argv[i]);
		if( !std::filesystem::exists(CurPath) )
		{
			std::fprintf(stderr, "File does not exist: %s\n", argv[i]);
			continue;
		}

		std::error_code CurError;

		std::uint32_t CRC32 = 0;

		if( std::filesystem::is_regular_file(CurPath, CurError) )
		{
			// Regular file sitting on some storage media, unchanging
			// Use a faster mmap path.
			const auto FileSize = std::filesystem::file_size(CurPath);
			const auto FileHandle = open(CurPath.c_str(), O_RDONLY, 0);
			const std::uint8_t* FileMap = (const std::uint8_t*)mmap(
				nullptr, FileSize,
				PROT_READ, MAP_FILE | MAP_SHARED,
				FileHandle, 0
			);
			CRC32 = Checksum<const std::uint8_t*>(
				FileMap, FileMap + FileSize
			);
			munmap((void*)FileMap, FileSize);
			close(FileHandle);
		}
		else
		{
			std::ifstream CurFile(CurPath, std::ios::binary);
			CRC32 = Checksum(
				std::istreambuf_iterator<char>(CurFile),
				std::istreambuf_iterator<char>()
			);
		}
		std::fputs(CurPath.c_str(), stdout);

		CurSettings.InputFiles.emplace_back(CurPath);
		std::printf(" %X\n", CRC32);
		// CurSettings.InputFile = fopen(argv[optind],"rb");
		// if( CurSettings.InputFile == nullptr )
		// {
		// 	std::fprintf(
		// 		stderr, "Error opening input file: %s\n", argv[optind]
		// 	);
		// }
		// return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
