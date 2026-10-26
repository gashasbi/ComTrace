cmake_minimum_required(VERSION 3.24)

project(ComTrace VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

add_executable(ComTrace
    src/main.c
)

if(MSVC)
    target_compile_options(ComTrace PRIVATE /W4 /WX /utf-8)
else()
    target_compile_options(ComTrace PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

include(CTest)

if(BUILD_TESTING)
    add_test(NAME comtrace_smoke COMMAND ComTrace --self-test)
endif()
