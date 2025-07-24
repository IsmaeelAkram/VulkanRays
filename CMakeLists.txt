cmake_minimum_required(VERSION 3.8)
project(VulkanRays LANGUAGES CXX)

# — C++ settings —
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_EXTENSIONS OFF)

# — Require VULKAN_SDK env var —
if (NOT DEFINED ENV{VULKAN_SDK})
  message(FATAL_ERROR "Please set VULKAN_SDK to your Vulkan SDK install path")
endif()
set(VK_SDK "$ENV{VULKAN_SDK}")

# — Gather sources —
file(GLOB_RECURSE SOURCES VulkanRays/*.cpp VulkanRays/*.h)

# — Executable —
add_executable(VulkanRays ${SOURCES})

# — Include dirs —
target_include_directories(VulkanRays PRIVATE
  "${VK_SDK}/Include"
  "${VK_SDK}/Include/SDL2"
)

# — Link libraries —
if (WIN32)
  set(VK_LIB "${VK_SDK}/Lib/vulkan-1.lib")
  set(SDL2_LIB "${VK_SDK}/Lib/SDL2.lib")
  set(SDL2MAIN_LIB "${VK_SDK}/Lib/SDL2main.lib")
else()
  # adjust for Linux/macOS if you unpack SDL2 yourself
  find_library(VULKAN_LIBRARY vulkan)
  find_library(SDL2_LIBRARY SDL2)
  set(VK_LIB ${VULKAN_LIBRARY})
  set(SDL2_LIB ${SDL2_LIBRARY})
  set(SDL2MAIN_LIB "")
endif()

target_link_libraries(VulkanRays PRIVATE
  ${VK_LIB}
  ${SDL2_LIB}
  ${SDL2MAIN_LIB}
)

# — Platform define —
if (WIN32)
  target_compile_definitions(VulkanRays PRIVATE PLATFORM_WINDOWS)
endif()
