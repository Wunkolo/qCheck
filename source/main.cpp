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

	if( CurSettings.Check )
		return Check(CurSettings);

	// Parse file list
	std::fprintf(
		stdout,
		"; Generated with qCheck by Wunkolo [ Build: " __TIMESTAMP__ " ]\n");

	for( const auto& CurPath : CurSettings.InputFiles )
	{
		std::error_code   CurError;
		const std::size_t FileSize = std::filesystem::file_size(CurPath);

		char        TimeString[64] = {0};
		struct stat FileStat       = {};
		if( stat(CurPath.c_str(), &FileStat) == 0 )
		{
			time_t FileTime = {};
			FileTime        = FileStat.st_mtime;
			std::strftime(
				TimeString, std::extent_v<decltype(TimeString)>, "%F %T %Z",
				std::localtime(&FileTime));
		}
		std::fprintf(
			stdout, "; %.64s %zu %s\n", TimeString, FileSize,
			CurPath.filename().c_str());
	}

	std::atomic<std::size_t> FileIndex(0);
	std::vector<std::thread> Workers;
	for( std::size_t i = 0; i < CurSettings.Threads; ++i )
	{
		Workers.push_back(std::thread(
			[&FileIndex, &FileList = std::as_const(CurSettings.InputFiles)](
				std::size_t WorkerIndex) {
#ifdef _POSIX_VERSION
				char ThreadName[16] = {0};
				std::snprintf(
					ThreadName, std::size(ThreadName), "qCheckWkr: %4zu",
					WorkerIndex);
#if defined(__APPLE__)
				pthread_setname_np(ThreadName);
#else
				pthread_setname_np(pthread_self(), ThreadName);
#endif
#endif
				while( true )
				{
					const std::size_t EntryIndex = FileIndex.fetch_add(1);
					if( EntryIndex >= FileList.size() )
						return;
					const std::filesystem::path& CurPath = FileList[EntryIndex];
					const std::optional<std::uint32_t> CRC32
						= ChecksumFile(CurPath);
					// If writing to a terminal, put some pretty colored output
					if( CRC32.has_value() )
					{
						if( isatty(fileno(stdout)) )
						{
							std::fprintf(
								stdout, "\e[36m%s\t\e[33m%08X\e[0m\n",
								CurPath.filename().c_str(), CRC32.value());
						}
						else
						{
							std::fprintf(
								stdout, "%s %08X\n", CurPath.filename().c_str(),
								CRC32.value());
						}
					}
					else
					{
						if( isatty(fileno(stdout)) )
						{
							std::fprintf(
								stdout, "\e[36m%s\t\e[31mERROR\e[0m\n",
								CurPath.filename().c_str());
						}
						else
						{
							std::fprintf(
								stdout, "%s ERROR\n",
								CurPath.filename().c_str());
						}
					}
				}
			},
			i));
	}
	for( std::size_t i = 0; i < CurSettings.Threads; ++i )
		Workers[i].join();

	return EXIT_SUCCESS;
}