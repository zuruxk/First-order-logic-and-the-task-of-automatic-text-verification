# Сборка дополнений. Корневой CMakeLists.txt и исходный код проекта не меняются:
# здесь те же src/*.cpp собираются в библиотеку, а рядом — новые методы, бенчмарк и тесты.
#
# Из корня проекта:
#   cmake -S extensions -B build-ext
#   cmake --build build-ext
cmake_minimum_required(VERSION 3.16)
project(TextVerificationExtensions LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release)   # замеры времени имеют смысл только с оптимизацией
endif()

set(ROOT ${CMAKE_CURRENT_SOURCE_DIR}/..)

# исходный код проекта (без src/main.cpp)
add_library(fol_core STATIC
    ${ROOT}/src/tokenizer.cpp
    ${ROOT}/src/nlp_parser.cpp
    ${ROOT}/src/logic.cpp
    ${ROOT}/src/resolution.cpp
    ${ROOT}/src/other_methods.cpp)
target_include_directories(fol_core PUBLIC ${ROOT}/include)

# новые методы
add_library(fol_ext STATIC
    src/ext_common.cpp
    src/resolution_sos.cpp
    src/forward_unit.cpp
    src/tableau_clausal.cpp
    src/hilbert.cpp)
target_include_directories(fol_ext PUBLIC include)
target_link_libraries(fol_ext PUBLIC fol_core)
if(NOT MSVC)
    target_compile_options(fol_ext PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(text_verification_ext src/main_ext.cpp)
target_link_libraries(text_verification_ext PRIVATE fol_ext)

add_executable(bench ${ROOT}/bench.cpp)
target_link_libraries(bench PRIVATE fol_ext)

add_executable(ext_tests tests/tests.cpp)
target_link_libraries(ext_tests PRIVATE fol_ext)
