# Zależności zewnętrzne.
# Najpierw szukamy bibliotek zainstalowanych w systemie (find_package),
# a jeśli ich nie ma - FetchContent pobiera je automatycznie podczas konfiguracji.
include(FetchContent)

# --- nlohmann/json: zapis i odczyt danych w formacie JSON ----------------------
FetchContent_Declare(nlohmann_json
  URL https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
  FIND_PACKAGE_ARGS 3.11
)
FetchContent_MakeAvailable(nlohmann_json)

# --- Boost: Program_options (linia poleceń) i String_algo (napisy) -------------
# Budujemy tylko potrzebne moduły - cała biblioteka Boost jest bardzo duża.
set(BOOST_INCLUDE_LIBRARIES program_options algorithm)
set(BOOST_ENABLE_CMAKE ON)
FetchContent_Declare(Boost
  URL https://github.com/boostorg/boost/releases/download/boost-1.89.0/boost-1.89.0-cmake.tar.xz
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
  FIND_PACKAGE_ARGS 1.80 CONFIG COMPONENTS program_options
)
FetchContent_MakeAvailable(Boost)

# Systemowy Boost udostępnia nagłówki jako Boost::headers, pobrany - jako osobne moduły.
if(TARGET Boost::algorithm)
  set(SKARBONKA_BOOST_HEADERS Boost::algorithm)
else()
  set(SKARBONKA_BOOST_HEADERS Boost::headers)
endif()

# --- MinGW: biblioteka wątków jest dostępna tylko jako DLL --------------------
# Kopiujemy ją obok pliku wykonywalnego, żeby program działał poza IDE.
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
