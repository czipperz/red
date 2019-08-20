project(red)
cmake_minimum_required(VERSION 2.6)

add_subdirectory(cz)

set(LIBRARY_NAME ${PROJECT_NAME}l)
file(GLOB_RECURSE SRCS src/*.cpp)
add_library(${LIBRARY_NAME} ${SRCS})
target_include_directories(${LIBRARY_NAME} PUBLIC cz/include)

add_executable(${PROJECT_NAME} main.cpp)
target_link_libraries(${PROJECT_NAME} ${LIBRARY_NAME})
target_link_libraries(${PROJECT_NAME} cz)

file(GLOB_RECURSE TEST_SRCS tests/*.cpp)
set(TEST_EXECUTABLE test)
add_executable(${TEST_EXECUTABLE} ${TEST_SRCS})
target_include_directories(${TEST_EXECUTABLE} PUBLIC cz/include)
target_include_directories(${TEST_EXECUTABLE} PUBLIC src)
target_link_libraries(${TEST_EXECUTABLE} ${LIBRARY_NAME})
target_link_libraries(${TEST_EXECUTABLE} cz)
