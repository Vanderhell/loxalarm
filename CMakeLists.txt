cmake_minimum_required(VERSION 3.20)
project(loxalarm VERSION 0.1.2 LANGUAGES C)

include(GNUInstallDirs)

add_library(loxalarm INTERFACE)
add_library(loxalarm::loxalarm ALIAS loxalarm)
target_include_directories(loxalarm INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_compile_features(loxalarm INTERFACE c_std_99)

option(LOXALARM_BUILD_TESTS "Build loxalarm unit tests" ON)
option(LOXALARM_BUILD_EXAMPLES "Build loxalarm examples" ON)
option(LOXALARM_WARNINGS_AS_ERRORS "Treat warnings as errors for repo targets" OFF)
option(LOXALARM_ENABLE_SANITIZERS "Enable ASan/UBSan (clang/gcc only)" OFF)

if (LOXALARM_BUILD_TESTS)
    include(CTest)

    add_executable(loxalarm_tests tests/test_loxalarm.c)
    target_link_libraries(loxalarm_tests PRIVATE loxalarm)

    add_executable(loxalarm_tests_fuzzlike tests/test_loxalarm_fuzzlike.c)
    target_link_libraries(loxalarm_tests_fuzzlike PRIVATE loxalarm)

    if (BUILD_TESTING)
        add_test(NAME loxalarm.unit COMMAND loxalarm_tests)
        add_test(NAME loxalarm.fuzzlike COMMAND loxalarm_tests_fuzzlike)
    endif()
endif()

if (LOXALARM_BUILD_EXAMPLES)
    add_executable(loxalarm_example_minimal examples/minimal.c)
    target_link_libraries(loxalarm_example_minimal PRIVATE loxalarm)

    add_executable(loxalarm_example_shelve_and_persist examples/shelve_and_persist.c)
    target_link_libraries(loxalarm_example_shelve_and_persist PRIVATE loxalarm)
endif()

set(_loxalarm_repo_targets "")
if (TARGET loxalarm_tests)
    list(APPEND _loxalarm_repo_targets loxalarm_tests)
endif()
if (TARGET loxalarm_tests_fuzzlike)
    list(APPEND _loxalarm_repo_targets loxalarm_tests_fuzzlike)
endif()
if (TARGET loxalarm_example_minimal)
    list(APPEND _loxalarm_repo_targets loxalarm_example_minimal)
endif()
if (TARGET loxalarm_example_shelve_and_persist)
    list(APPEND _loxalarm_repo_targets loxalarm_example_shelve_and_persist)
endif()

if (LOXALARM_WARNINGS_AS_ERRORS)
    foreach(tgt IN LISTS _loxalarm_repo_targets)
        if (MSVC)
            target_compile_options(${tgt} PRIVATE /W4 /WX)
        else()
            target_compile_options(${tgt} PRIVATE -Wall -Wextra -Wpedantic -Werror)
        endif()
    endforeach()
endif()

if (LOXALARM_ENABLE_SANITIZERS)
    if (MSVC)
        message(FATAL_ERROR "Sanitizers option is for clang/gcc toolchains.")
    endif()
    foreach(tgt IN LISTS _loxalarm_repo_targets)
        target_compile_options(${tgt} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
        target_link_options(${tgt} PRIVATE -fsanitize=address,undefined)
    endforeach()
endif()

install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(TARGETS loxalarm EXPORT loxalarmTargets)

include(CMakePackageConfigHelpers)
set(_loxalarm_cmakedir "${CMAKE_INSTALL_LIBDIR}/cmake/loxalarm")
configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/loxalarmConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/loxalarmConfig.cmake"
    INSTALL_DESTINATION "${_loxalarm_cmakedir}"
)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/loxalarmConfigVersion.cmake"
    VERSION "${PROJECT_VERSION}"
    COMPATIBILITY SameMajorVersion
)

install(EXPORT loxalarmTargets
    NAMESPACE loxalarm::
    FILE loxalarmTargets.cmake
    DESTINATION "${_loxalarm_cmakedir}"
)
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/loxalarmConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/loxalarmConfigVersion.cmake"
    DESTINATION "${_loxalarm_cmakedir}"
)
