cmake_minimum_required(VERSION 3.20)
project(loxalarm C)

include(CTest)

add_library(loxalarm INTERFACE)
target_include_directories(loxalarm INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(loxalarm INTERFACE c_std_99)

add_executable(loxalarm_tests tests/test_loxalarm.c)
target_link_libraries(loxalarm_tests PRIVATE loxalarm)

add_executable(loxalarm_tests_fuzzlike tests/test_loxalarm_fuzzlike.c)
target_link_libraries(loxalarm_tests_fuzzlike PRIVATE loxalarm)

add_executable(loxalarm_example_minimal examples/minimal.c)
target_link_libraries(loxalarm_example_minimal PRIVATE loxalarm)

add_executable(loxalarm_example_shelve_and_persist examples/shelve_and_persist.c)
target_link_libraries(loxalarm_example_shelve_and_persist PRIVATE loxalarm)

if (BUILD_TESTING)
    add_test(NAME loxalarm.unit COMMAND loxalarm_tests)
    add_test(NAME loxalarm.fuzzlike COMMAND loxalarm_tests_fuzzlike)
endif()

option(LOXALARM_ENABLE_SANITIZERS "Enable ASan/UBSan (clang/gcc only)" OFF)
if (LOXALARM_ENABLE_SANITIZERS)
    if (MSVC)
        message(FATAL_ERROR "Sanitizers option is for clang/gcc toolchains.")
    endif()
    foreach(tgt loxalarm_tests loxalarm_tests_fuzzlike loxalarm_example_minimal loxalarm_example_shelve_and_persist)
        target_compile_options(${tgt} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
        target_link_options(${tgt} PRIVATE -fsanitize=address,undefined)
    endforeach()
endif()
