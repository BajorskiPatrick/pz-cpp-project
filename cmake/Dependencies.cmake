# External dependencies.
# Libraries installed in the system are looked up first (find_package);
# if they are missing, FetchContent downloads them automatically during configuration.
include(FetchContent)

# --- nlohmann/json: saving and loading data in JSON format ----------------------
FetchContent_Declare(nlohmann_json
  URL https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
  FIND_PACKAGE_ARGS 3.11
)
FetchContent_MakeAvailable(nlohmann_json)

# --- Boost: Program_options (command line) and String_algo (strings) -----------
# Only the required modules are built - the whole Boost library is very large.
set(BOOST_INCLUDE_LIBRARIES program_options algorithm)
set(BOOST_ENABLE_CMAKE ON)
FetchContent_Declare(Boost
  URL https://github.com/boostorg/boost/releases/download/boost-1.89.0/boost-1.89.0-cmake.tar.xz
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
  FIND_PACKAGE_ARGS 1.80 CONFIG COMPONENTS program_options
)
FetchContent_MakeAvailable(Boost)

# A system Boost exposes its headers as Boost::headers, a downloaded one as separate modules.
if(TARGET Boost::algorithm)
  set(SKARBONKA_BOOST_HEADERS Boost::algorithm)
else()
  set(SKARBONKA_BOOST_HEADERS Boost::headers)
endif()

# --- MinGW: the threading library is only available as a DLL ------------------
# Copy it next to the executable so the program also runs outside the IDE.
function(skarbonka_copy_runtime target)
  if(MINGW)
    get_filename_component(compiler_dir "${CMAKE_CXX_COMPILER}" DIRECTORY)
    set(winpthread "${compiler_dir}/libwinpthread-1.dll")
    if(EXISTS "${winpthread}")
      add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${winpthread}" "$<TARGET_FILE_DIR:${target}>"
      )
    endif()
  endif()
endfunction()
