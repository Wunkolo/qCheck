#include <algorithm>
#include <charconv>
#include <cstring>
#include <thread>

#include <sys/stat.h>
#include <unistd.h>

#include <CRC/CRC32.hpp>

#include <qCheck.hpp>

namespace
{

void ProcessInputPath(Settings& CurSettings, const std::filesystem::path& Path)
{
	if( !std::filesystem::exists(Path) )
	{
		std::fprintf(stderr, "Path does not exist: %s\n", Path.c_str());
		return;
	}
	std::error_code CurError;

	if( CurSettings.Check )
	{
		// Only add .sfv files
		if( std::filesystem::is_regular_file(Path, CurError) )
		{
			const auto Extension = Path.extension();
			if( Path.has_extension() && Extension.compare(".sfv") == 0 )
			{
				CurSettings.InputFiles.emplace_back(Path);
			}
		}
		// Add .sfv files to check recursively
		else if(
			CurSettings.Recursive
			&& std::filesystem::is_directory(Path, CurError) )
		{
			for( const std::filesystem::directory_entry& DirectoryEntry :
				 std::filesystem::directory_iterator(Path) )
			{
				ProcessInputPath(CurSettings, DirectoryEntry.path());
			}
		}
		else
		{
			std::fprintf(stderr, "Error opening path: %s\n", Path.c_str());
		}
	}
	else
	{
		if( std::filesystem::is_regular_file(Path, CurError) )
		{
			CurSettings.InputFiles.emplace_back(Path);
		}
		else
		{
			std::fprintf(stderr, "Error opening path: %s\n", Path.c_str());
		}
	}
}

} // namespace

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
	while(
		(Opt = getopt_long(argc, argv, "t:crh", CommandOptions, &OptionIndex))
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
		case 'r':
		{
			CurSettings.Recursive = true;
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
		ProcessInputPath(CurSettings, CurPath);
	}

	return CurSettings.Check ? CheckSFVs(CurSettings)
							 : GenerateSFV(CurSettings);
}