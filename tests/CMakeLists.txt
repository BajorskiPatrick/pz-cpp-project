FetchContent_Declare(googletest
  URL https://github.com/google/googletest/releases/download/v1.17.0/googletest-1.17.0.tar.gz
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
  FIND_PACKAGE_ARGS NAMES GTest
)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

include(GoogleTest)

add_executable(skarbonka_tests
  AppTest.cpp
  BudgetTest.cpp
  CliTest.cpp
  DateTest.cpp
  MoneyTest.cpp
  ReportTest.cpp
  StorageTest.cpp
)
target_link_libraries(skarbonka_tests PRIVATE skarbonka_core GTest::gtest_main)
skarbonka_warnings(skarbonka_tests)
skarbonka_copy_runtime(skarbonka_tests)

gtest_discover_tests(skarbonka_tests DISCOVERY_MODE PRE_TEST)
