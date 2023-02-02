cmake_minimum_required( VERSION 3.7 )
project( qCheck )

### Standard
set( CMAKE_CXX_STANDARD 20 )
set( CMAKE_CXX_STANDARD_REQUIRED ON )
set( CMAKE_CXX_EXTENSIONS OFF )

### Verbosity
set( CMAKE_COLOR_MAKEFILE ON )
set( CMAKE_EXPORT_COMPILE_COMMANDS ON )

### Packaging
if( NOT DEFINED QCHECK_SNAPSHOT_TAG )
	execute_process(
		COMMAND git rev-parse --short HEAD
		WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
		OUTPUT_VARIABLE _GIT_SHORT_HASH
		OUTPUT_STRIP_TRAILING_WHITESPACE
	)
	execute_process(
		COMMAND git rev-list --count --first-parent HEAD
		WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
		OUTPUT_VARIABLE _GIT_REV_COUNT
		OUTPUT_STRIP_TRAILING_WHITESPACE
	)
	set( QCHECK_SNAPSHOT_TAG "${_GIT_REV_COUNT}.git${_GIT_SHORT_HASH}" )
endif()

set( CPACK_GENERATOR "RPM;DEB" )

set( CPACK_PACKAGE_VERSION "0" )
set( CPACK_PACKAGE_VENDOR "Wunkolo" )
set( CPACK_PACKAGE_DESCRIPTION "qCheck accelerates the checksum and checksum-verification process by interfacing with the .sfv file format and using a more modern approach by utilizing both multi-threaded parallelism and memory-mapped IO to reduce syscall overhead." )
set( CPACK_PACKAGE_DESCRIPTION_SUMMARY "Generate and verify CRC32C checksum files" )

set( CPACK_PACKAGING_INSTALL_PREFIX "/usr" )

set( CPACK_RPM_PACKAGE_NAME "qCheck" )
set( CPACK_RPM_PACKAGE_VERSION "0^${QCHECK_SNAPSHOT_TAG}" )
set( CPACK_RPM_PACKAGE_RELEASE_DIST ON )
set( CPACK_RPM_PACKAGE_LICENSE "MIT" )
set( CPACK_RPM_PACKAGE_GROUP "System Tools" )
set( CPACK_RPM_FILE_NAME "RPM-DEFAULT" )
set( CPACK_RPM_BUILDREQUIRES "cmake >= 3.7, gcc-c++, catch-devel >= 2" )
set( CPACK_RPM_SOURCE_PKG_BUILD_PARAMS "-DCMAKE_BUILD_TYPE=RelWithDebInfo -DQCHECK_SNAPSHOT_TAG=${QCHECK_SNAPSHOT_TAG}" )
set( CPACK_RPM_DEBUGINFO_PACKAGE ON )
set( CPACK_RPM_BUILD_SOURCE_DIRS_PREFIX "/usr/src/debug/${CPACK_RPM_PACKAGE_NAME}-${CPACK_RPM_PACKAGE_VERSION}" )

set( CPACK_DEBIAN_PACKAGE_NAME "qcheck" )
set( CPACK_DEBIAN_PACKAGE_VERSION "0+${QCHECK_SNAPSHOT_TAG}" )
set( CPACK_DEBIAN_PACKAGE_MAINTAINER "Wunkolo <wunkolo@gmail.com>" )
set( CPACK_DEBIAN_PACKAGE_SECTION "utils" )
set( CPACK_DEBIAN_FILE_NAME "DEB-DEFAULT" )

include( CPack )

### Optimizations

if( MSVC )
	add_compile_options(
		/MP # Parallel builds 
		/permissive- # Stricter C++ conformance

		# Warnings
		/W3

		# Consider these warnings as errors
		/we4018 # 'expression': signed/unsigned mismatch
		/we4062 # Enumerator 'identifier' in a switch of enum 'enumeration' is not handled
		/we4101 # 'identifier': unreferenced local variable
		/we4265 # 'class': class has virtual functions, but destructor is not virtual
		/we4305 # 'context': truncation from 'type1' to 'type2'
		/we4388 # 'expression': signed/unsigned mismatch
		/we4389 # 'operator': signed/unsigned mismatch

		/we4456 # Declaration of 'identifier' hides previous local declaration
		/we4457 # Declaration of 'identifier' hides function parameter
		/we4458 # Declaration of 'identifier' hides class member
		/we4459 # Declaration of 'identifier' hides global declaration

		/we4505 # 'function': unreferenced local function has been removed
		/we4547 # 'operator': operator before comma has no effect; expected operator with side-effect
		/we4549 # 'operator1': operator before comma has no effect; did you intend 'operator2'?
		/we4555 # Expression has no effect; expected expression with side-effect
		/we4715 # 'function': not all control paths return a value
		/we4834 # Discarding return value of function with 'nodiscard' attribute
		/we5038 # data member 'member1' will be initialized after data member 'member2'
		/we5245 # 'function': unreferenced function with internal linkage has been removed

	)
elseif( CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang" )
	# Force colored diagnostic messages in Ninja's output
	if( CMAKE_GENERATOR STREQUAL "Ninja" )
		add_compile_options( -fdiagnostics-color=always )
	endif()

	# add_compile_options(-march=native)

	add_compile_options(
		-Wall
		-Warray-bounds
		-Wextra
		-Wimplicit-fallthrough
		-Wmissing-declarations
		-Wmissing-declarations
		-Wmissing-field-initializers
		-Wno-attributes
		-Wno-invalid-offsetof
		-Wno-unused-parameter
		-Wreorder
		-Wshadow
		-Wsign-compare
		-Wswitch
		-Wuninitialized
		-Wunused-function
		-Wunused-result
		-Wunused-variable
	)
endif()

add_executable(
	qCheck
	source/main.cpp
)

install(TARGETS qCheck DESTINATION bin)

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

### Tests
enable_testing()

find_package(Catch2 3 REQUIRED)

add_executable(
	CRC32_test
	tests/CRC32.cpp
)
target_link_libraries(
	CRC32_test
	PRIVATE
	Catch2::Catch2WithMain
)
target_include_directories(
	CRC32_test
	PRIVATE
	include
)

add_test(CRC32_test CRC32_test)