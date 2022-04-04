cmake_minimum_required(VERSION 3.21)
project(vulkan)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

set(SRC_DIR ${CMAKE_SOURCE_DIR}/src)
set(SRC_DIR_VULKAN ${SRC_DIR}/vulkan)
set(SRC_DIR_SHADERS ${SRC_DIR}/shaders)
set(SRC_FILES
        ${SRC_DIR}/main.cpp
        ${SRC_DIR}/window.cpp
        ${SRC_DIR}/shader_registry.cpp
        ${SRC_DIR}/file_utils.cpp
        ${SRC_DIR}/shader_paths.cpp
        ${SRC_DIR_VULKAN}/device_builder.cpp
        ${SRC_DIR_VULKAN}/instance_builder.cpp
        ${SRC_DIR_VULKAN}/pipeline_builder.cpp
        ${SRC_DIR_VULKAN}/swapchain_builder.cpp
        ${SRC_DIR_VULKAN}/buffer.cpp)
set(SHADER_SRC_FILES
        ${SRC_DIR_SHADERS}/color_passthrough.frag
        ${SRC_DIR_SHADERS}/simple2d.vert)
add_executable(vulkan ${SRC_FILES})

# https://stackoverflow.com/questions/2368811/how-to-set-warning-level-in-cmake
if (MSVC)
    target_compile_options(vulkan PRIVATE /W4 /WX)
else ()
    target_compile_options(vulkan PRIVATE -Wall -Wextra -Wpedantic)
endif ()
set_property(TARGET vulkan PROPERTY CXX_STANDARD 20)

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
add_subdirectory(deps/glfw3)

find_package(Vulkan REQUIRED COMPONENTS glslc)
find_package(glm 0.9.9 REQUIRED)

find_program(glslc_executable NAMES glslc HINTS Vulkan::glslc)

foreach (FILE ${SHADER_SRC_FILES})
    get_filename_component(FILE_NAME ${FILE} NAME)
    set(OUT_FILE ${CMAKE_SOURCE_DIR}/bin/shaders/${FILE_NAME}.spv)
    add_custom_command(
            OUTPUT ${OUT_FILE}
            COMMAND
            ${glslc_executable}
            --target-env=vulkan1.0
            -o ${OUT_FILE}
            ${FILE}
    )
    list(APPEND COMPILED_SHADERS ${OUT_FILE})
endforeach ()

add_custom_target(ShaderCompile DEPENDS ${COMPILED_SHADERS})
add_dependencies(vulkan ShaderCompile)

target_include_directories(vulkan PUBLIC ${Vulkan_INCLUDE_DIRS} ${GLM_INCLUDE_DIRS})
target_link_libraries(vulkan PRIVATE ${Vulkan_LIBRARIES} glfw)
#Cmake can't pass macros, but (void)(expr) kind of works as a noop
target_compile_definitions(vulkan PRIVATE
        GLFW_INCLUDE_VULKAN
        VULKAN_HPP_NO_CONSTRUCTORS
        VULKAN_HPP_NO_EXCEPTIONS
        VULKAN_HPP_ASSERT_ON_RESULT=\(void\))
