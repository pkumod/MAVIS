cmake_minimum_required(VERSION 3.16)
project(MAVIS)

include_directories(
        ${PROJECT_SOURCE_DIR}/utility
        ${PROJECT_SOURCE_DIR}/
)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2 -fPIC -std=c++17 -g -Wall -pthread -march=native -Werror=return-type")

add_subdirectory(graph)
add_subdirectory(utility)
add_subdirectory(view_matching)

# mavis executable
add_executable(mavis mavis.cpp)
target_link_libraries(mavis PRIVATE RewriteLib MatViewLib graph utility)
