cmake_minimum_required(VERSION 3.16)
project(pocketwiki_core_tests C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug)
endif()

set(REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/..")
set(CORE_DIR "${REPO_ROOT}/core")
set(COMPANION_DIR "${REPO_ROOT}/companion")

# --- inkkit device layer headers -------------------------------------------
# We depend on inkkit rather than reimplementing the device layer. Point the
# build at a local checkout with -DINKKIT_SRC_DIR=/path/to/inkkit/src, otherwise
# fetch it from GitHub. Only the Arduino free headers (ByteStream.h) are used by
# the host test build.
if(NOT DEFINED INKKIT_SRC_DIR)
  if(DEFINED ENV{INKKIT_SRC_DIR})
    set(INKKIT_SRC_DIR "$ENV{INKKIT_SRC_DIR}")
  endif()
endif()

if(NOT INKKIT_SRC_DIR)
  include(FetchContent)
  FetchContent_Declare(
    inkkit
    GIT_REPOSITORY https://github.com/mohitagw15856/inkkit.git
    GIT_TAG main
  )
  FetchContent_MakeAvailable(inkkit)
  set(INKKIT_SRC_DIR "${inkkit_SOURCE_DIR}/src")
endif()

message(STATUS "Using inkkit headers from: ${INKKIT_SRC_DIR}")

# --- core library -----------------------------------------------------------
add_library(pocketwiki_core STATIC
  ${CORE_DIR}/pocketwiki/PwaReader.cpp
  ${CORE_DIR}/third_party/puff/puff.c
)
target_include_directories(pocketwiki_core PUBLIC
  ${CORE_DIR}
  ${CORE_DIR}/third_party/puff
  ${INKKIT_SRC_DIR}
)
target_compile_options(pocketwiki_core PRIVATE -Wall -Wextra)

# --- generate the mini interop fixture with the companion -------------------
find_package(Python3 COMPONENTS Interpreter REQUIRED)
set(MINI_PWA "${CMAKE_BINARY_DIR}/mini.pwa")
add_custom_command(
  OUTPUT ${MINI_PWA}
  COMMAND ${CMAKE_COMMAND} -E env PYTHONPATH=${COMPANION_DIR}
          ${Python3_EXECUTABLE} -m pocketwiki build markdown
          ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/mini -o ${MINI_PWA} -n Mini
  DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/mini/alpha.md
    ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/mini/beta.md
    ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/mini/gamma.md
  COMMENT "Building mini interop archive with the companion"
  VERBATIM
)
add_custom_target(gen_mini DEPENDS ${MINI_PWA})

set(SAMPLE_PWA "${COMPANION_DIR}/sample/foundations-of-science.pwa")

enable_testing()

function(add_core_test name)
  add_executable(${name} ${name}.cpp)
  target_link_libraries(${name} PRIVATE pocketwiki_core)
  target_compile_options(${name} PRIVATE -Wall -Wextra)
  add_test(NAME ${name} COMMAND ${name})
endfunction()

add_core_test(test_fold)
add_core_test(test_render)
add_core_test(test_library)
add_core_test(test_text)
add_core_test(test_layout)

# Widget tests also need the firmware src/ headers (Arduino free parts).
add_executable(test_widgets test_widgets.cpp)
target_link_libraries(test_widgets PRIVATE pocketwiki_core)
target_include_directories(test_widgets PRIVATE ${REPO_ROOT}/src)
target_compile_options(test_widgets PRIVATE -Wall -Wextra)
add_test(NAME test_widgets COMMAND test_widgets)

add_executable(test_reader test_reader.cpp)
target_link_libraries(test_reader PRIVATE pocketwiki_core)
target_compile_options(test_reader PRIVATE -Wall -Wextra)
add_dependencies(test_reader gen_mini)
target_compile_definitions(test_reader PRIVATE
  POCKETWIKI_MINI_PWA="${MINI_PWA}"
  POCKETWIKI_SAMPLE_PWA="${SAMPLE_PWA}"
)
add_test(NAME test_reader COMMAND test_reader)
