#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <charconv>
#include <algorithm>
#include <numeric>
#include <mutex>
#include <atomic>
#include <thread>

#include <vector>
#include <queue>
#include <filesystem>
#include <fstream>
#include <iterator>

#include <getopt.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sched.h>
#include <pthread.h>

#include "CRC32.hpp"

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
"  -t, --threads            Number of checker threads in parallel\n"
"  -c, --check              Verify an .sfv file\n";

const static struct option CommandOptions[] = {
	{ "threads",  optional_argument, nullptr,  't' },
	{ "help",     optional_argument, nullptr,  'h' },
	{ "check",    optional_argument, nullptr,  'c' },
	{ nullptr,          no_argument, nullptr, '\0' }
};

std::uint32_t ChecksumFile( const std::filesystem::path& Path )
{
	std::uint32_t CRC32 = 0;
	std::error_code CurError;
	// Regular file sitting on some storage media, unchanging
	// Use a faster mmap path.
	if( std::filesystem::is_regular_file(Path, CurError) )
	{
		const std::uintmax_t FileSize = std::filesystem::file_size(Path);
		const auto FileHandle = open(Path.c_str(), O_RDONLY, 0);
		void* FileMap = mmap( nullptr, FileSize,
			PROT_READ, MAP_SHARED | MAP_POPULATE, FileHandle, 0
		);
		madvise(FileMap, FileSize, MADV_SEQUENTIAL | MADV_WILLNEED);
		CRC32 = CRC::Checksum<0xEDB88320u, const std::uint8_t*>(
			reinterpret_cast<const std::uint8_t*>(FileMap),
			reinterpret_cast<const std::uint8_t*>(FileMap) + FileSize
		);
		munmap((void*)FileMap, FileSize);
		close(FileHandle);
	}
	else
	{
		std::ifstream CurFile(Path, std::ios::binary);
		CRC32 = CRC::Checksum<0xEDB88320u>(
			std::istreambuf_iterator<char>(CurFile),
			std::istreambuf_iterator<char>()
		);
	}

	return CRC32;
}

int Check(const Settings& CurSettings)
{
	struct CheckEntry
	{
		std::filesystem::path FilePath;
		std::uint32_t Checksum;
	};
	std::atomic<std::size_t> QueueLock;
	std::vector<CheckEntry> Checkqueue;
	std::string CurLine;
	std::ifstream CheckFile(CurSettings.ChecksumFile);
	if( !CheckFile ) return EXIT_FAILURE;
	while( std::getline(CheckFile, CurLine) )
	{
		if( CurLine[0] == ';' ) continue;
		const std::size_t BreakPos = CurLine.find_last_of(' ');
		const std::string_view CheckString = std::string_view(CurLine).substr(BreakPos + 1);
		std::uint32_t CheckValue = ~0u;
		const std::from_chars_result ParseResult = std::from_chars<std::uint32_t>(
			CheckString.begin(), CheckString.end(), CheckValue, 16
		);
		if( ParseResult.ec != std::errc() )
		{
			// Error parsing checksum value
			continue;
		}
		Checkqueue.push_back(
			{
				CurSettings.ChecksumFile.parent_path() / std::string_view(CurLine).substr(0, BreakPos),
				CheckValue
			}
		);
	}

	std::vector<std::thread> Workers;

	for( std::size_t i = 0; i < CurSettings.Threads; ++i )
	{
		Workers.push_back( std::thread(
			[&QueueLock, &Checkqueue = std::as_const(Checkqueue)]
			(std::size_t WorkerIndex)
			{
			#ifdef _POSIX_VERSION
				char ThreadName[16] = {0};
				std::sprintf(ThreadName, "qCheckWkr: %4zu", WorkerIndex);
				pthread_setname_np(pthread_self(), ThreadName);
			#endif
				while( true )
				{
					const std::size_t EntryIndex = std::atomic_fetch_add(
						&QueueLock, 1
					);
					if( EntryIndex >= Checkqueue.size()) return;
					const CheckEntry& CurEntry = Checkqueue[EntryIndex];
					if( std::filesystem::is_regular_file(CurEntry.FilePath) )
					{
						const std::uint32_t CurSum = ChecksumFile(CurEntry.FilePath);
						const bool Valid = CurEntry.Checksum == CurSum;
						std::printf(
							"\e[36m%s\t\e[33m%08X\e[37m...%s%08X\t%s\e[0m\n",
							CurEntry.FilePath.c_str(), CurEntry.Checksum,
							Valid ? "\e[32m" : "\e[31m", CurSum,
							Valid ? "\e[32mOK" : "\e[31mFAIL"
						);
					}
					else
					{
						std::printf(
							"\e[36m%s\t\e[33m%08X\t\t\e[31mError opening file\n",
							CurEntry.FilePath.c_str(), CurEntry.Checksum
						);
					}
				}
			}, i)
		);
	}

	for( std::size_t i = 0; i < CurSettings.Threads; ++i ) Workers[i].join();

	return EXIT_SUCCESS;
}

int main( int argc, char* argv[] )
{
	Settings CurSettings = {};
	int Opt;
	int OptionIndex;
	CurSettings.Threads = std::max<std::size_t>(
		std::thread::hardware_concurrency() / 4, CurSettings.Threads
	);
	if( argc <= 1 )
	{
		std::puts(Usage);
		return EXIT_SUCCESS;
	}
	// Parse Arguments
	while( (Opt = getopt_long(argc, argv, "c:t:h", CommandOptions, &OptionIndex )) != -1 )
	{
		switch( Opt )
		{
		case 't':
		{
			std::size_t Threads;
			const auto ParseResult = std::from_chars<std::size_t>(
				optarg, optarg + std::strlen(optarg), Threads
			);
			if( *ParseResult.ptr != '\0' || ParseResult.ec != std::errc() )
			{
				std::fprintf(stdout, "Invalid thread count \"%s\"\n", optarg);
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
		case 'c':
		{
			CurSettings.ChecksumFile = std::filesystem::path(optarg);
			break;
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

	if( !CurSettings.ChecksumFile.empty() ) return Check(CurSettings);

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
		// Regular files only, for now, other files will be specially handled later
		if( std::filesystem::is_regular_file(CurPath, CurError) )
		{
			FileSize = std::filesystem::file_size(CurPath);

			char TimeString[64] = {0};
			struct stat FileStat = {};
			if( stat(CurPath.c_str(), &FileStat) == 0 )
			{
				time_t FileTime = {};
				FileTime = FileStat.st_mtime;
				std::strftime(
					TimeString, std::extent_v<decltype(TimeString)>,
					"%F %T %Z", std::localtime(&FileTime)
				);
			}
			CurSettings.InputFiles.emplace_back(CurPath);
			std::fprintf(
				stdout, "; %.64s %zu %s\n",
				TimeString, FileSize,
				CurPath.filename().c_str()
			);
		}
	}

	std::atomic<std::size_t> FileIndex(0);
	std::vector<std::thread> Workers;
	for( std::size_t i = 0; i < CurSettings.Threads; ++i )
	{
		Workers.push_back( std::thread(
			[&FileIndex, &FileList = std::as_const(CurSettings.InputFiles)]
			(std::size_t WorkerIndex)
			{
			#ifdef _POSIX_VERSION
				char ThreadName[16] = {0};
				std::sprintf(ThreadName, "qCheckWkr: %4zu", WorkerIndex);
				pthread_setname_np(pthread_self(), ThreadName);
			#endif
				while( true )
				{
					const std::size_t EntryIndex = std::atomic_fetch_add(
						&FileIndex, 1
					);
					if( EntryIndex >= FileList.size()) return;
					const std::filesystem::path& CurPath = FileList[EntryIndex];
					const std::uint32_t CRC32 = ChecksumFile(CurPath);
					// If writing to a terminal, put some pretty colored output
					if( isatty(fileno(stdout)) )
					{
						std::fprintf(stdout, "\e[36m%s\t\e[33m%08X\e[0m\n", CurPath.filename().c_str(), CRC32);
					}
					else
					{
						std::fprintf(stdout, "%s %08X\n", CurPath.filename().c_str(), CRC32);
					}
				}
			}, i)
		);
	}
	for( std::size_t i = 0; i < CurSettings.Threads; ++i ) Workers[i].join();

	return EXIT_SUCCESS;
}
