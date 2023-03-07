#include <charconv>
#include <cstring>
#include <thread>

#include <sys/stat.h>
#include <unistd.h>

#include <CRC/CRC32.hpp>

#include <qCheck.hpp>

int main(int argc, char* argv[])
{
	Settings CurSettings = {};
	int      Opt;
	int      OptionIndex;
	CurSettings.Threads = std::max<std::size_t>(
		{std::thread::hardware_concurrency() / 4, CurSettings.Threads, 1});

	if( argc <= 1 )
	{
		std::puts(Usage);
		return EXIT_SUCCESS;
	}
	// Parse Arguments
	while( (Opt = getopt_long(argc, argv, "t:ch", CommandOptions, &OptionIndex))
		   != -1 )
	{
		switch( Opt )
		{
		case 't':
		{
			std::size_t Threads;
			const auto  ParseResult = std::from_chars<std::size_t>(
                optarg, optarg + std::strlen(optarg), Threads);
			if( *ParseResult.ptr != '\0' || ParseResult.ec != std::errc() )
			{
				std::fprintf(stdout, "Invalid thread count \"%s\"\n", optarg);
				return EXIT_FAILURE;
			}
			CurSettings.Threads = static_cast<std::size_t>(Threads);
			break;
		}
		case 'c':
		{
			CurSettings.Check = true;
			break;
		}
		case 'h':
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

	for( std::intmax_t i = 0; i < argc; ++i )
	{
		const std::filesystem::path CurPath(argv[i]);
		if( !std::filesystem::exists(CurPath) )
		{
			std::fprintf(stderr, "File does not exist: %s\n", argv[i]);
			continue;
		}
		std::error_code CurError;
		// Regular files only, for now, other files will be specially handled
		// later
		if( std::filesystem::is_regular_file(CurPath, CurError) )
		{
			CurSettings.InputFiles.emplace_back(CurPath);
		}
		else
		{
			std::fprintf(stderr, "Error opening file: %s\n", argv[i]);
		}
	}

	return CurSettings.Check ? CheckSFV(CurSettings) : GenerateSFV(CurSettings);
}