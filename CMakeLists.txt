project(red)
cmake_minimum_required(VERSION 2.6)

add_subdirectory(cz)

include_directories(cz/include)
include_directories(tracy)

set(LIBRARY_NAME ${PROJECT_NAME}l)
file(GLOB_RECURSE SRCS src/*.cpp)
add_library(${LIBRARY_NAME} ${SRCS})

add_executable(${PROJECT_NAME} main.cpp)
target_link_libraries(${PROJECT_NAME} ${LIBRARY_NAME})
target_link_libraries(${PROJECT_NAME} tracy)
target_link_libraries(${PROJECT_NAME} cz)
target_link_libraries(${PROJECT_NAME} pthread)
target_link_libraries(${PROJECT_NAME} dl)

file(GLOB_RECURSE TEST_SRCS tests/*.cpp)
set(TEST_EXECUTABLE test)
add_executable(${TEST_EXECUTABLE} ${TEST_SRCS})
target_include_directories(${TEST_EXECUTABLE} PUBLIC src)
target_include_directories(${TEST_EXECUTABLE} PUBLIC cz/test_base)
target_link_libraries(${TEST_EXECUTABLE} ${LIBRARY_NAME})
target_link_libraries(${TEST_EXECUTABLE} tracy)
target_link_libraries(${TEST_EXECUTABLE} cz)
target_link_libraries(${TEST_EXECUTABLE} czt)
target_link_libraries(${TEST_EXECUTABLE} pthread)
target_link_libraries(${TEST_EXECUTABLE} dl)

add_library(tracy tracy/TracyClient.cpp)

if (TRACY_ENABLE)
  add_compile_definitions(TRACY_ENABLE)
endif()
