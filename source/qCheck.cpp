#include <qCheck.hpp>

#include <charconv>
#include <fstream>
#include <set>
#include <span>
#include <thread>

#include <CRC/CRC32.hpp>

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

const char* Usage
	= "qCheck - Wunkolo <wunkolo@gmail.com>\n"
	  "Usage: qCheck [Options]... [Files]...\n"
	  "  -t, --threads            Number of checker threads in parallel\n"
	  "  -c, --check              Verify all input as .sfv files\n"
	  "  -h, --help               Show this help message\n";

static std::optional<std::uint32_t>
	ChecksumFile(const std::filesystem::path& Path)
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

		CRC32 = CRC::Checksum(FileData);

		munmap((void*)FileMap, FileSize);
	}
	else
	{
		std::array<std::byte, 4096> Buffer;

		ssize_t ReadCount = read(FileHandle, Buffer.data(), Buffer.size());
		while( ReadCount > 0 )
		{
			CRC32
				= CRC::Checksum(std::span(Buffer).subspan(0, ReadCount), CRC32);
			ReadCount = read(FileHandle, Buffer.data(), Buffer.size());
		}
	}

	close(FileHandle);

	return CRC32;
}

struct CheckEntry
{
	std::filesystem::path FilePath;
	std::size_t           InputIndex;
	std::uint32_t         Checksum;
};

static void CheckerThread(
	std::atomic<std::size_t>& Passed, std::atomic<std::size_t>& QueueLock,
	std::span<const CheckEntry> Checkqueue, std::span<std::uint8_t> ErrorList,
	std::size_t WorkerIndex)
{
#ifdef _POSIX_VERSION
	char ThreadName[16] = {0};
	std::snprintf(
		ThreadName, std::size(ThreadName), "qCheckWkr: %4zu", WorkerIndex);

#if defined(__APPLE__)
	pthread_setname_np(ThreadName);
#else
	pthread_setname_np(pthread_self(), ThreadName);
#endif
#endif

	while( true )
	{
		const std::size_t EntryIndex
			= QueueLock.fetch_add(1, std::memory_order_relaxed);
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

			if( Valid )
			{
				Passed.fetch_add(1, std::memory_order_relaxed);
			}
			else
			{
				// Todo: Error code for checksum mismatch
				ErrorList[EntryIndex] = 1;
			}
		}
		else
		{
			// Todo: Error code for unable to checksum file
			ErrorList[EntryIndex] = 2;

			std::printf(
				"\e[36m%s\t\e[33m%08X\t\t\e[31mError opening "
				"file\n",
				CurEntry.FilePath.c_str(), CurEntry.Checksum);
		}
	}
}

int CheckSFVs(const Settings& CurSettings)
{
	std::atomic<std::size_t> QueueLock{0};
	std::vector<CheckEntry>  Checkqueue;
	// Queue up all files to be checked

	std::string CurLine;
	for( std::size_t InputIndex = 0; InputIndex < CurSettings.InputFiles.size();
		 ++InputIndex )
	{
		const std::filesystem::path& CurSfvPath
			= CurSettings.InputFiles[InputIndex];

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
			Checkqueue.push_back(CheckEntry{
				.FilePath   = FilePath,
				.InputIndex = InputIndex,
				.Checksum   = CheckValue,
			});
		}
	}

	std::vector<std::thread> Workers;
	std::atomic<std::size_t> Passed{0};

	// Todo: Error Codes
	std::vector<std::uint8_t> ErrorList(Checkqueue.size(), 0);

	for( std::size_t i = 0; i < CurSettings.Threads; ++i )
	{
		Workers.push_back(std::thread(
			CheckerThread, std::ref(Passed), std::ref(QueueLock),
			std::span(Checkqueue), std::span(ErrorList), i));
	}

	for( std::thread& Worker : Workers )
	{
		Worker.join();
	}

	if( Checkqueue.size() == Passed.load() )
	{
		return EXIT_SUCCESS;
	}
	else
	{
		if( Checkqueue.size() > 1 )
		{
			std::set<std::size_t> FailInputs;

			// Gather all inputs that lead to a failure
			for( std::size_t EntryIndex = 0; EntryIndex < Checkqueue.size();
				 ++EntryIndex )
			{
				// This entry failed
				if( ErrorList[EntryIndex] > 0 )
				{
					const std::size_t InputIndex
						= Checkqueue[EntryIndex].InputIndex;
					FailInputs.emplace(InputIndex);
				}
			}

			std::fputs("\nFailed SFVs:\n", stdout);
			for( const std::size_t& InputIndex : FailInputs )
			{
				const std::filesystem::path& FailedInput
					= CurSettings.InputFiles[InputIndex];
				std::fprintf(stdout, "\e[31m%s\e[0m\n", FailedInput.c_str());
			}
		}
		return EXIT_FAILURE;
	}
}

static void GenCheckThread(
	std::atomic<std::size_t>&              FileIndex,
	std::span<const std::filesystem::path> FileList, std::size_t WorkerIndex)
{

#ifdef _POSIX_VERSION
	char ThreadName[16] = {0};
	std::snprintf(
		ThreadName, std::size(ThreadName), "qCheckWkr: %4zu", WorkerIndex);
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
		const std::filesystem::path&       CurPath = FileList[EntryIndex];
		const std::optional<std::uint32_t> CRC32   = ChecksumFile(CurPath);
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
				std::fprintf(stdout, "%s ERROR\n", CurPath.filename().c_str());
			}
		}
	}
}

int GenerateSFV(const Settings& CurSettings)
{
	// SFV Header
	// https://en.wikipedia.org/wiki/Simple_file_verification

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
			&GenCheckThread, std::ref(FileIndex), CurSettings.InputFiles, i));
	}

	for( std::thread& Worker : Workers )
	{
		Worker.join();
	}

	return EXIT_SUCCESS;
}