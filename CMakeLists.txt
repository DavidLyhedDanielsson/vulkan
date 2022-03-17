cmake_minimum_required(VERSION 3.22)
project(vulkan)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

file(GLOB SRC_FILES src/*.cpp)
add_executable(vulkan ${SRC_FILES})

# https://stackoverflow.com/questions/2368811/how-to-set-warning-level-in-cmake
if(MSVC)
  target_compile_options(vulkan PRIVATE /W4 /WX)
else()
  target_compile_options(vulkan PRIVATE -Wall -Wextra -Wpedantic)
endif()
set_property(TARGET vulkan PROPERTY CXX_STANDARD 20)

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
add_subdirectory(deps/glfw3)

find_package(Vulkan REQUIRED)
find_package(glm 0.9.9 REQUIRED)

target_include_directories(vulkan PUBLIC ${Vulkan_INCLUDE_DIRS} ${GLM_INCLUDE_DIRS})
target_link_libraries(vulkan PRIVATE ${Vulkan_LIBRARIES} glfw)
target_compile_definitions(vulkan PRIVATE GLFW_INCLUDE_VULKAN)
