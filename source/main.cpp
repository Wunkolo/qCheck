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

struct Settings
{
	std::filesystem::path ChecksumFile;
	std::vector<std::filesystem::path> InputFiles;
	std::size_t Threads   = 2;
	bool Verbose = true;
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

constexpr std::array<std::uint32_t, 256> CRC32Table(
   std::uint32_t Polynomial
) noexcept
{
	std::array<std::uint32_t, 256> Result = {};
	std::uint32_t n = 0;
	for( std::size_t i = 0; i < 256; ++i )
	{
		std::uint32_t CurChecksum = n++;
		for( std::size_t j = 0; j < 8; ++j )
		{
			CurChecksum = (CurChecksum >> 1) ^ (
				(CurChecksum & 0x1u) ? Polynomial : 0
			);
		}
		Result[i] = CurChecksum;
	}
	return Result;
}
 
template< typename InputIterator >
std::uint32_t crc(InputIterator First, InputIterator Last)
{
  static auto const Table = CRC32Table(0xEDB88320u);
 
  return std::uint32_t{0xFFFFFFFFuL} &
	~std::accumulate(First, Last,
	  ~std::uint32_t{0} & std::uint32_t{0xFFFFFFFFu},
		[](std::uint32_t CurChecksum, std::uint8_t CurValue) 
		  { return Table[static_cast<std::uint8_t>(CurChecksum ^ CurValue)] ^ (CurChecksum >> 8); });
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
		if( std::filesystem::is_regular_file(CurPath, CurError) )
		{
			// Regular file sitting on some storage media, unchanging
			// Use a faster mmap path.
		}
		std::fputs(CurPath.c_str(), stdout);

		CurSettings.InputFiles.emplace_back(CurPath);

		std::ifstream CurFile(CurPath, std::ios::binary);
		const std::uint32_t CRC32 = crc(
			std::istream_iterator<char>(CurFile),
			std::istream_iterator<char>()
		);
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
