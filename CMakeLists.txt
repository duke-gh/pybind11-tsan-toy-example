cmake_minimum_required(VERSION 3.20)
project(embed_test LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(PY_PREFIX "$ENV{HOME}/repos/python-tsan")
set(Python_ROOT_DIR "${PY_PREFIX}" CACHE PATH "" FORCE)
set(Python_EXECUTABLE "${PY_PREFIX}/bin/python3" CACHE FILEPATH "" FORCE)
set(Python_INCLUDE_DIR "${PY_PREFIX}/include/python3.12d" CACHE PATH "" FORCE)

set(Python_LIBRARY "${PY_PREFIX}/lib/libpython3.12d.so" CACHE FILEPATH "" FORCE)


find_package(Python REQUIRED COMPONENTS Interpreter Development Development.Embed)

set(PYBIND11_FINDPYTHON ON CACHE BOOL "" FORCE)

# Point CMake at your pybind11 checkout (not installed).
add_subdirectory(../pybind11 pybind11-build)



find_package(Python COMPONENTS Interpreter Development REQUIRED)

add_executable(embed_test src/main.cpp)

# pybind11::embed sets up include dirs + correct Python linking
target_link_libraries(embed_test PRIVATE pybind11::embed)


# TSan flags (compile + link)
target_compile_options(embed_test PRIVATE
    -fsanitize=thread -g -O1 -fno-omit-frame-pointer
)
target_link_options(embed_test PRIVATE
    -fsanitize=thread
)
