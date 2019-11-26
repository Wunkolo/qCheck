#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <charconv>
#include <algorithm>

#include <vector>
#include <filesystem>

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
			std::fprintf(
				stderr,
				"Unable to open file: %s\n",
				argv[i]
			);
		}
		std::printf(
			"%s : %s\n",
			CurPath.c_str(),
			 ? "Exists":"Doesn't exist"
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
