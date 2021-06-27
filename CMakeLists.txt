cmake_minimum_required(VERSION 3.9)
project(slco C)

# C standard
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED YES)
set(CMAKE_C_EXTENSIONS NO)

# Try to compile release
set(DEFAULT_BUILD_TYPE "Release")
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
	message(STATUS "Setting build type to '${DEFAULT_BUILD_TYPE}' as none was specified.")
	set(CMAKE_BUILD_TYPE "${DEFAULT_BUILD_TYPE}" CACHE STRING "Choose the type of build." FORCE)
	# Set the possible values of build type for cmake-gui
	set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()

# Header only library
add_library(slco INTERFACE)
target_include_directories(slco INTERFACE ${PROJECT_SOURCE_DIR}/include)

option(SLCO_EXAMPLES_BUILD_SAN "Build with sanitizers" OFF)

option(SLCO_EXAMPLES_BUILD "Build example programs" OFF)
mark_as_advanced(FORCE SLCO_EXAMPLES_BUILD)
if(SLCO_EXAMPLES_BUILD)
	if(SLCO_EXAMPLES_BUILD_SAN)
		target_compile_options(slco INTERFACE -fsanitize=address,leak,undefined,pointer-compare,pointer-subtract -fstack-protector)
		target_link_options   (slco INTERFACE -fsanitize=address,leak,undefined,pointer-compare,pointer-subtract -fstack-protector)
	endif()
        add_subdirectory(examples/generator)
endif()
