cmake_minimum_required( VERSION 3.2.2 )
project( qCheck )

### Verbosity
set( CMAKE_COLOR_MAKEFILE ON )
set( CMAKE_EXPORT_COMPILE_COMMANDS ON )

### Optimizations
if( MSVC )
	add_compile_options( /W4 )
elseif( CMAKE_COMPILER_IS_GNUCXX )
	add_compile_options( -Wall )
	add_compile_options( -Wextra )
endif()

### Tests
enable_testing()

add_executable(
	qCheck
	source/main.cpp
)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
target_link_libraries(
	qCheck
	PRIVATE
	Threads::Threads
)
target_include_directories(
	qCheck
	PRIVATE
	include
)
set_target_properties(
	qCheck
	PROPERTIES
	CXX_STANDARD 17
	CXX_STANDARD_REQUIRED YES
	CXX_EXTENSIONS OFF
)
