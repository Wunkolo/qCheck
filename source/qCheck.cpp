#include <qCheck.hpp>

#include <charconv>
#include <fstream>
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
	std::uint32_t         Checksum;
};

void CheckerThread(
	std::atomic<std::size_t>& Passed, std::atomic<std::size_t>& QueueLock,
	std::span<const CheckEntry> Checkqueue, std::size_t WorkerIndex)
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

			Passed.fetch_add(Valid, std::memory_order_relaxed);
		}
		else
		{
			std::printf(
				"\e[36m%s\t\e[33m%08X\t\t\e[31mError opening "
				"file\n",
				CurEntry.FilePath.c_str(), CurEntry.Checksum);
		}
	}
}

int Check(const Settings& CurSettings)
{
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
	std::atomic<std::size_t> Passed{0};

	for( std::size_t i = 0; i < CurSettings.Threads; ++i )
	{
		Workers.push_back(std::thread(
			CheckerThread, std::ref(Passed), std::ref(QueueLock),
			std::span(Checkqueue), i));
	}

	for( std::thread& Worker : Workers )
	{
		Worker.join();
	}

	return Checkqueue.size() == Passed.load() ? EXIT_SUCCESS : EXIT_FAILURE;
}