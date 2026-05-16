cmake_minimum_required(VERSION 3.25 FATAL_ERROR)

project(sys_prog LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/allocator/allocator/include)

message([[VCPKG toolchain setup example. It is needed for some libraries. Highly recommended to pass it by command line arguments for cmake configuration (IDE can help with this).
        set(VCPKG_TARGET_TRIPLET x64-windows)
        set(CMAKE_TOOLCHAIN_FILE C:/Programming/vcpkg/scripts/buildsystems/vcpkg.cmake)]])

include(FetchContent)
FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip)
FetchContent_MakeAvailable(googletest)

find_package(Boost COMPONENTS container) # Ставить через vcpkg

add_subdirectory(allocator)

if (Boost_FOUND)
    message(STATUS "Boost found, building associative_container")
    add_subdirectory(associative_container)
else ()
    message(WARNING "Boost not found, associative_container will be skipped")
endif ()

add_subdirectory(common)