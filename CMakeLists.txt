cmake_minimum_required(VERSION 3.16)
project(SystemHealthToolkit VERSION 3.0.0 LANGUAGES CXX)

# C++20 required for std::filesystem, <chrono>, etc.
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# ─── Options ───────────────────────────────────
option(SHT_BUILD_STATIC "Build static binary" OFF)
option(SHT_ENABLE_TESTS "Build test suite" OFF)

# ─── Sources ───────────────────────────────────
set(SHT_SOURCES
    src/main.cpp
    src/logger.cpp
    src/platform.cpp
    src/threadpool.cpp
    src/ui.cpp
    src/repair.cpp
    src/storage.cpp
    src/scanners.cpp
    src/benchmark.cpp
    src/new_ops.cpp
    src/malware.cpp
)

set(SHT_HEADERS
    include/toolkit.h
    include/logger.h
    include/platform.h
    include/ui.h
    include/repair.h
    include/scanners.h
)

# ─── Include dirs ──────────────────────────────
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# ─── Threads ───────────────────────────────────
find_package(Threads REQUIRED)

# ─── Executable ────────────────────────────────
if(SHT_BUILD_STATIC)
    set(CMAKE_FIND_LIBRARY_SUFFIXES .a)
    set(BUILD_SHARED_LIBS OFF)
    set(CMAKE_EXE_LINKER_FLAGS "-static-libgcc -static-libstdc++")
endif()

add_executable(SystemHealthToolkit ${SHT_SOURCES} ${SHT_HEADERS})

target_link_libraries(SystemHealthToolkit PRIVATE Threads::Threads)

if(WIN32)
    # Note: wuapi.h (Windows Update Agent COM interfaces) needs no import lib --
    # it's consumed via CoCreateInstance from ole32, not linked directly.
    target_link_libraries(SystemHealthToolkit PRIVATE
        shell32 ole32 oleaut32 wbemuuid userenv advapi32 pdh)
    set_target_properties(SystemHealthToolkit PROPERTIES
        WIN32_EXECUTABLE FALSE  # Console app, not GUI
        LINK_FLAGS "/MANIFEST:NO /LARGEADDRESSAWARE")
elseif(APPLE)
    target_link_libraries(SystemHealthToolkit PRIVATE
        "-framework CoreFoundation -framework IOKit")
    set_target_properties(SystemHealthToolkit PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.systemhealth.toolkit"
        MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
        MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR})
else()
    target_link_libraries(SystemHealthToolkit PRIVATE pthread dl rt)
endif()

# ─── Platform-specific compile defs ────────────
if(WIN32)
    target_compile_definitions(SystemHealthToolkit PRIVATE _WIN32_WINNT=0x0A00 NOMINMAX)
endif()

# ─── Install ───────────────────────────────────
install(TARGETS SystemHealthToolkit DESTINATION bin)
install(FILES ${SHT_HEADERS} DESTINATION include/SystemHealthToolkit)
install(FILES
    launcher.bat
    launcher.ps1
    run.sh
    README.md
    LICENSE
    DESTINATION bin
)

# ─── Tests (optional) ─────────────────────────
if(SHT_ENABLE_TESTS)
    enable_testing()
    find_package(GTest QUIET)
    if(GTest_FOUND)
        add_executable(sht_tests
            tests/test_main.cpp
            tests/test_platform.cpp
            tests/test_repair.cpp
        )
        target_link_libraries(sht_tests PRIVATE GTest::GTest Threads::Threads)
        add_test(NAME sht_tests COMMAND sht_tests)
    else()
        message(WARNING "GTest not found — tests disabled")
    endif()
endif()

# ─── Packaging ─────────────────────────────────
set(CPACK_PACKAGE_NAME "SystemHealthToolkit")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Cross-Platform System Health & Repair Toolkit")
set(CPACK_PACKAGE_VENDOR "System Health Toolkit Team")
set(CPACK_RESOURCE_FILE_LICENSE ${CMAKE_CURRENT_SOURCE_DIR}/LICENSE)
set(CPACK_RESOURCE_FILE_README ${CMAKE_CURRENT_SOURCE_DIR}/README.md)

if(WIN32)
    set(CPACK_GENERATOR "NSIS;ZIP")
elseif(APPLE)
    set(CPACK_GENERATOR "DragNDrop;TGZ")
else()
    set(CPACK_GENERATOR "DEB;RPM;TGZ")
endif()

include(CPack)

# ─── Custom target: run ───────────────────────
add_custom_target(run
    COMMAND SystemHealthToolkit
    DEPENDS SystemHealthToolkit
    USES_TERMINAL
)

# ─── Custom target: clean-all ─────────────────
add_custom_target(clean-all
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}/Logs
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}/Reports
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}/Temp
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}/Config
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}/Assets
    COMMENT "Removing generated data directories"
)

# ─── Message ───────────────────────────────────
message(STATUS "System Health Toolkit v${PROJECT_VERSION}")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "Sources: ${SHT_SOURCES}")