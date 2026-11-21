cmake_minimum_required(VERSION 3.24)
project(skarbonka VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release)
endif()

option(SKARBONKA_BUILD_TESTS "Build unit tests (GoogleTest)" ON)

# MinGW: link the standard libraries statically so the program can also be
# run outside the IDE (without libstdc++-6.dll in PATH).
if(MINGW)
  add_link_options(-static-libgcc -static-libstdc++)
endif()

include(cmake/Dependencies.cmake)

# --- Library with the program logic (shared by the application and tests) ---
add_library(skarbonka_core STATIC
  src/App.cpp
  src/Budget.cpp
  src/Cli.cpp
  src/Date.cpp
  src/Filter.cpp
  src/Money.cpp
  src/Storage.cpp
  src/Transaction.cpp
)
target_include_directories(skarbonka_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(skarbonka_core
  PUBLIC  Boost::program_options
  PRIVATE nlohmann_json::nlohmann_json ${SKARBONKA_BOOST_HEADERS}
)

# --- Console application -----------------------------------------------------
add_executable(skarbonka src/main.cpp)
target_link_libraries(skarbonka PRIVATE skarbonka_core)
if(WIN32)
  # CommandLineToArgvW (UTF-16 arguments -> UTF-8, see main.cpp).
  target_link_libraries(skarbonka PRIVATE shell32)
endif()
skarbonka_copy_runtime(skarbonka)

function(skarbonka_warnings target)
  if(MSVC)
    target_compile_options(${target} PRIVATE /W4 /permissive- /utf-8)
  else()
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Wshadow)
  endif()
endfunction()

skarbonka_warnings(skarbonka_core)
skarbonka_warnings(skarbonka)

# --- Tests --------------------------------------------------------------------
if(SKARBONKA_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
