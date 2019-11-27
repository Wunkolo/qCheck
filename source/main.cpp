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
	{ "threads",        optional_argument, nullptr,  't' },
	{ "help",           optional_argument, nullptr,  'h' },
	{ nullptr,                no_argument, nullptr, '\0' }
};

std::array<std::uint32_t, 256> generate_crc_lookup_table() noexcept
{
	const std::uint32_t reversed_polynomial = 0xEDB88320u;

	// This is a function object that calculates the checksum for a value,
	// then increments the value, starting from zero.
	struct byte_checksum
	{
		std::uint32_t operator()() noexcept
		{
			std::uint32_t CurChecksum = n++;

			for (std::size_t i = 0; i < 8; ++i)
			{
				CurChecksum = (CurChecksum >> 1) ^ ((CurChecksum & 0x1u) ? reversed_polynomial : 0);
			}

			return CurChecksum;
		}
		std::uint32_t n = 0;
	};

	std::array<std::uint32_t, 256> table = {};
	std::generate(table.begin(), table.end(), byte_checksum{});

	return table;
}
 
// Calculates the CRC for any sequence of values. (You could use type traits and a
// static assert to ensure the values can be converted to 8 bits.)
template <typename InputIterator>
std::uint32_t crc(InputIterator first, InputIterator last)
{
  // Generate lookup table only on first use then cache it - this is thread-safe.
  static auto const table = generate_crc_lookup_table();
 
  // Calculate the CurChecksum - make sure to clip to 32 bits, for systems that don't
  // have a true (fast) 32-bit type.
  return std::uint32_t{0xFFFFFFFFuL} &
    ~std::accumulate(first, last,
      ~std::uint32_t{0} & std::uint32_t{0xFFFFFFFFuL},
        [](std::uint32_t CurChecksum, std::uint8_t CurValue) 
          { return table[static_cast<std::uint8_t>(CurChecksum ^ CurValue)] ^ (CurChecksum >> 8); });
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
		std::printf(
			" %X\n",
			CRC32
		);
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
