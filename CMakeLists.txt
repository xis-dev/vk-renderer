cmake_minimum_required(VERSION 4.3)

project(vulkan-renderer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY  ${PROJECT_SOURCE_DIR}/lib)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include (${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders/shader_compile.cmake)
find_package(Vulkan REQUIRED)

include(FetchContent)

FetchContent_Declare(
        SDL3
        GIT_TAG release-3.4.18
        GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
        GIT_SHALLOW TRUE
)

set(SDL_SHARED OFF CACHE BOOL "" FORCE)   # no SDL3.dll / libSDL3.dylib to ship
set(SDL_STATIC ON  CACHE BOOL "" FORCE)   # link SDL3 into our executable
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL_TESTS OFF CACHE BOOL "" FORCE)
set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(SDL3)

FetchContent_Declare(
        volk
        GIT_TAG 1.4.350
        GIT_REPOSITORY https://github.com/zeux/volk.git
        GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(volk)

FetchContent_Declare(
        vma
        GIT_TAG v3.4.0
        GIT_REPOSITORY https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator.git
        GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(vma)

FetchContent_Declare(
        glm
        GIT_TAG 1.0.3
        GIT_REPOSITORY https://github.com/g-truc/glm.git
        GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(glm)

add_executable(${PROJECT_NAME} src/main.cpp
        src/engine.cpp
        src/pipeline_builder.cpp
        src/mesh.cpp)

target_compile_definitions(${PROJECT_NAME} PRIVATE "$<$<CONFIG:Debug>:DEBUG_MODE>")

target_include_directories(${PROJECT_NAME} PRIVATE
        ${PROJECT_SOURCE_DIR}
        ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(${PROJECT_NAME} PRIVATE
        SDL3::SDL3-static
        GPUOpen::VulkanMemoryAllocator
        glm::glm
        volk::volk
)

add_shaders(SHADERS
        assets/shaders/triangle.frag
        assets/shaders/triangle.vert
)

add_dependencies(${PROJECT_NAME} SHADERS)
