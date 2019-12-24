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
#include <sys/mman.h>
#include <fcntl.h>
#include <pthread.h>

#ifdef __AVX2__
#include <immintrin.h>
#endif

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
"  -t, --threads            Number of checker threads in parallel\n";

const static struct option CommandOptions[] = {
	{ "threads",  optional_argument, nullptr,  't' },
	{ "help",     optional_argument, nullptr,  'h' },
	{ "check",    optional_argument, nullptr,  'c' },
	{ nullptr,          no_argument, nullptr, '\0' }
};

constexpr std::array<std::array<std::uint32_t, 256>, 8> CRC32Table(
	std::uint32_t Polynomial
) noexcept
{
	std::array<std::array<std::uint32_t, 256>, 8> Table = {};
	for( std::size_t i = 0; i < 256; ++i )
	{
		std::uint32_t CRC = i;
		for( std::size_t CurBit = 0; CurBit < 8; ++CurBit )
		{
			CRC = (CRC >> 1) ^ ( -(CRC & 0b1) & Polynomial);
		}
		Table[0][i] = CRC;
	}

	for( std::size_t i = 0; i < 256; ++i )
	{
		Table[1][i] = (Table[0][i] >> 8) ^ Table[0][std::uint8_t(Table[0][i])];
		Table[2][i] = (Table[1][i] >> 8) ^ Table[0][std::uint8_t(Table[1][i])];
		Table[3][i] = (Table[2][i] >> 8) ^ Table[0][std::uint8_t(Table[2][i])];
		Table[4][i] = (Table[3][i] >> 8) ^ Table[0][std::uint8_t(Table[3][i])];
		Table[5][i] = (Table[4][i] >> 8) ^ Table[0][std::uint8_t(Table[4][i])];
		Table[6][i] = (Table[5][i] >> 8) ^ Table[0][std::uint8_t(Table[5][i])];
		Table[7][i] = (Table[6][i] >> 8) ^ Table[0][std::uint8_t(Table[6][i])];
	}

	return Table;
}

#ifdef __AVX2__
inline std::uint32_t _mm256_hxor_epi32(__m256i a)
{
	// Xor top half with bottom half
	const __m128i XorReduce128 = _mm_xor_si128(
		_mm256_extracti128_si256(a, 1), _mm256_extracti128_si256(a, 0)
	);
	const std::uint64_t XorReduce64 =
		_mm_extract_epi64(XorReduce128, 1) ^ _mm_extract_epi64(XorReduce128, 0);
	return XorReduce64 ^ (XorReduce64 >> 32);
}
#endif

// Todo: This should technically be "contiguous_iterator_tag" from C++20
template< typename RandomIterator, std::uint32_t Polynomial >
std::uint32_t Checksum(RandomIterator First, RandomIterator Last, std::random_access_iterator_tag)
{
	static constexpr auto Table = CRC32Table(Polynomial);
	std::uint32_t CRC = ~0;
	const std::size_t Size = static_cast<std::size_t>(std::distance(First, Last));
	const std::uint32_t* Input32 = reinterpret_cast<const std::uint32_t*>(&(*First));
	std::size_t i;

	// Slice by 8
#ifdef __AVX2__
	const __m256i ByteIndex = _mm256_set_epi8(
		~0, ~0, ~0, 7, ~0, ~0, ~0, 6, ~0, ~0, ~0, 5, ~0, ~0, ~0, 4,
		~0, ~0, ~0, 3, ~0, ~0, ~0, 2, ~0, ~0, ~0, 1, ~0, ~0, ~0, 0
	);
	const __m256i ArrayOffset = _mm256_set_epi32(
		(sizeof(typename decltype(Table)::value_type) / 4) * 0,
		(sizeof(typename decltype(Table)::value_type) / 4) * 1,
		(sizeof(typename decltype(Table)::value_type) / 4) * 2,
		(sizeof(typename decltype(Table)::value_type) / 4) * 3,
		(sizeof(typename decltype(Table)::value_type) / 4) * 4,
		(sizeof(typename decltype(Table)::value_type) / 4) * 5,
		(sizeof(typename decltype(Table)::value_type) / 4) * 6,
		(sizeof(typename decltype(Table)::value_type) / 4) * 7 
	);
	for( i = 0; i < Size / 8; ++i )
	{
		const std::uint64_t Input64 =
			*reinterpret_cast<const std::uint64_t*>(Input32) ^ CRC;
		Input32 += 2;
		const __m256i Indices = _mm256_shuffle_epi8(
			_mm256_set1_epi64x(Input64), ByteIndex
		);
		const __m256i Gather = _mm256_i32gather_epi32(
			reinterpret_cast<const std::int32_t*>(Table.data()),
			_mm256_add_epi32(Indices, ArrayOffset), sizeof(std::uint32_t)
		);
		CRC = _mm256_hxor_epi32(Gather);
	}
#else
	for( i = 0; i < Size / 8; ++i )
	{
		const std::uint32_t InputLo = *Input32++ ^ CRC;
		const std::uint32_t InputHi = *Input32++;
		CRC =
			Table[7][std::uint8_t(InputLo      )] ^
			Table[6][std::uint8_t(InputLo >>  8)] ^
			Table[5][std::uint8_t(InputLo >> 16)] ^
			Table[4][std::uint8_t(InputLo >> 24)] ^
			Table[3][std::uint8_t(InputHi      )] ^
			Table[2][std::uint8_t(InputHi >>  8)] ^
			Table[1][std::uint8_t(InputHi >> 16)] ^
			Table[0][std::uint8_t(InputHi >> 24)];
	}
#endif

	First += (i * 8);
	return ~std::accumulate( First, Last, CRC,
		[](std::uint32_t CRC, std::uint8_t Byte) 
		{
			return (CRC >> 8) ^ Table[0][std::uint8_t(CRC) ^ Byte];
		}
	);
}

template< typename Iterator, std::uint32_t Polynomial >
std::uint32_t Checksum(Iterator First, Iterator Last, std::input_iterator_tag)
{
	static constexpr auto Table = CRC32Table(Polynomial);
	return ~std::accumulate( First, Last, ~std::uint32_t(0),
		[](std::uint32_t CRC, std::uint8_t Byte) 
		{
			return (CRC >> 8) ^ Table[0][std::uint8_t(CRC) ^ Byte];
		}
	);
}

template< std::uint32_t Polynomial, typename Iterator>
inline std::uint32_t Checksum(Iterator First, Iterator Last)
{
	return Checksum<Iterator, Polynomial>( First, Last,
		typename std::iterator_traits<Iterator>::iterator_category()
	);
}

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
		CRC32 = Checksum<0xEDB88320u, const std::uint8_t*>(
			reinterpret_cast<const std::uint8_t*>(FileMap),
			reinterpret_cast<const std::uint8_t*>(FileMap) + FileSize
		);
		munmap((void*)FileMap, FileSize);
		close(FileHandle);
	}
	else
	{
		std::ifstream CurFile(Path, std::ios::binary);
		CRC32 = Checksum<0xEDB88320u>(
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
			{ std::string_view(CurLine).substr(0, BreakPos), CheckValue }
		);
	}

	std::vector<std::thread> Workers;

	for( std::size_t i = 0; i < CurSettings.Threads; ++i )
	{
		Workers.push_back( std::thread(
			[&QueueLock, &Checkqueue = std::as_const(Checkqueue)]
			(std::size_t Index)
			{
				pthread_setname_np( pthread_self(),
					("qCheck-Worker: " + std::to_string(Index)).c_str()
				);
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
			CurSettings.InputFiles.emplace_back(CurPath);
			std::fprintf(stdout, "; %zu %s\n", FileSize, CurPath.filename().c_str());
		}
	}

	for( const auto& CurPath : CurSettings.InputFiles )
	{
		const std::uint32_t CRC32 = ChecksumFile(CurPath);
		std::fprintf(stdout, "%s %08X\n", CurPath.filename().c_str(), CRC32);
	}
	return EXIT_SUCCESS;
}
