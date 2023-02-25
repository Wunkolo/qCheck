#include <algorithm>
#include <atomic>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <numeric>
#include <thread>

#include <filesystem>
#include <fstream>
#include <optional>
#include <queue>
#include <utility>
#include <vector>

#include <fcntl.h>
#include <getopt.h>
#include <pthread.h>
#include <sched.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "CRC32.hpp"

struct Settings
{
	std::vector<std::filesystem::path> InputFiles;
	std::size_t                        Threads = 2;
	bool                               Verbose = true;
	bool                               Check   = false;
};

const char* Usage
	= "qCheck - Wunkolo <wunkolo@gmail.com>\n"
	  "Usage: qCheck [Options]... [Files]...\n"
	  "  -t, --threads            Number of checker threads in parallel\n"
	  "  -c, --check              Verify all input as .sfv files\n"
	  "  -h, --help               Show this help message\n";

const static struct option CommandOptions[]
	= {{"threads", required_argument, nullptr, 't'},
	   {"check", no_argument, nullptr, 'c'},
	   {"help", no_argument, nullptr, 'h'},
	   {nullptr, no_argument, nullptr, '\0'}};

std::optional<std::uint32_t> ChecksumFile(const std::filesystem::path& Path);
int                          Check(const Settings& CurSettings);

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
				std::sprintf(ThreadName, "qCheckWkr: %4zu", WorkerIndex);
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

std::optional<std::uint32_t> ChecksumFile(const std::filesystem::path& Path)
{
	std::uint32_t     CRC32 = 0;
	std::error_code   CurError;
	const std::size_t FileSize = std::filesystem::file_size(Path, CurError);

	if( CurError )
	{
		return std::nullopt;
	}

	const int FileHandle = open(Path.c_str(), O_RDONLY, 0);
	if( FileHandle == -1 )
	{
		return std::nullopt;
	}

// Try to map the file, upon failure, use regular file-descriptor reads
#if defined(__APPLE__)
	void* FileMap
		= mmap(nullptr, FileSize, PROT_READ, MAP_SHARED, FileHandle, 0);
#else
	void* FileMap = mmap(
		nullptr, FileSize, PROT_READ, MAP_SHARED | MAP_POPULATE, FileHandle, 0);
#endif

	if( std::uintptr_t(FileMap) != -1ULL )
	{
		const auto FileData = std::span<const std::byte>(
			reinterpret_cast<const std::byte*>(FileMap), FileSize);

		madvise(FileMap, FileSize, MADV_SEQUENTIAL | MADV_WILLNEED);

		CRC32 = CRC::Checksum<0xEDB88320u>(FileData);

		munmap((void*)FileMap, FileSize);
	}
	else
	{
		std::array<std::byte, 4096> Buffer;

		ssize_t ReadCount = read(FileHandle, Buffer.data(), Buffer.size());
		while( ReadCount > 0 )
		{
			CRC32 = CRC::Checksum<0xEDB88320u>(
				std::span(Buffer).subspan(0, ReadCount), CRC32);
			ReadCount = read(FileHandle, Buffer.data(), Buffer.size());
		}
	}

	close(FileHandle);

	return CRC32;
}

int Check(const Settings& CurSettings)
{
	struct CheckEntry
	{
		std::filesystem::path FilePath;
		std::uint32_t         Checksum;
	};
	std::atomic<std::size_t> QueueLock{0};
	std::vector<CheckEntry>  Checkqueue;

	// Queue up all files to be checked

	std::string CurLine;
	for( const auto& CurSfvPath : CurSettings.InputFiles )
	{
		std::ifstream CheckFile(CurSfvPath);
		if( !CheckFile )
		{
			std::fprintf(
				stdout, "Failed to open \"%s\" for reading\n",
				CurSfvPath.string().c_str());
			return EXIT_FAILURE;
		}

		while( std::getline(CheckFile, CurLine) )
		{
			if( CurLine[0] == ';' )
				continue;
			const std::size_t      BreakPos = CurLine.find_last_of(' ');
			const std::string_view PathString
				= std::string_view(CurLine).substr(0, BreakPos);
			const std::string_view CheckString
				= std::string_view(CurLine).substr(BreakPos + 1);
			std::uint32_t                CheckValue = ~0u;
			const std::from_chars_result ParseResult
				= std::from_chars<std::uint32_t>(
					CheckString.begin(), CheckString.end(), CheckValue, 16);
			if( ParseResult.ec != std::errc() )
			{
				// Error parsing checksum value
				continue;
			}
			std::filesystem::path FilePath;
			if( CurSfvPath.has_parent_path() )
			{
				FilePath = CurSfvPath.parent_path();
			}
			else
			{
				FilePath = ".";
			}
			FilePath /= PathString;
			Checkqueue.push_back({FilePath, CheckValue});
		}
	}

	std::vector<std::thread> Workers;

	for( std::size_t i = 0; i < CurSettings.Threads; ++i )
	{
		Workers.push_back(std::thread(
			[&QueueLock,
			 &Checkqueue = std::as_const(Checkqueue)](std::size_t WorkerIndex) {
#ifdef _POSIX_VERSION
				char ThreadName[16] = {0};
				std::sprintf(ThreadName, "qCheckWkr: %4zu", WorkerIndex);
#if defined(__APPLE__)
				pthread_setname_np(ThreadName);
#else
				pthread_setname_np(pthread_self(), ThreadName);
#endif
#endif
				while( true )
				{
					const std::size_t EntryIndex = QueueLock.fetch_add(1);
					if( EntryIndex >= Checkqueue.size() )
						return;
					const CheckEntry& CurEntry = Checkqueue[EntryIndex];

					const std::optional<std::uint32_t> CurSum
						= ChecksumFile(CurEntry.FilePath);

					if( CurSum.has_value() )
					{
						const bool Valid = CurEntry.Checksum == CurSum;
						std::printf(
							"\e[36m%s\t\e[33m%08X\e[37m...%s%08X\t%s\e[0m\n",
							CurEntry.FilePath.c_str(), CurEntry.Checksum,
							Valid ? "\e[32m" : "\e[31m", CurSum.value(),
							Valid ? "\e[32mOK" : "\e[31mFAIL");
					}
					else
					{
						std::printf(
							"\e[36m%s\t\e[33m%08X\t\t\e[31mError opening "
							"file\n",
							CurEntry.FilePath.c_str(), CurEntry.Checksum);
					}
				}
			},
			i));
	}

	for( std::size_t i = 0; i < CurSettings.Threads; ++i )
		Workers[i].join();

	return EXIT_SUCCESS;
}