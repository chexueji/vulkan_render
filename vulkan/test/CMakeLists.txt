cmake_minimum_required(VERSION 3.0 FATAL_ERROR)
project(nnr_vulkan)

option(VR_BUILD_VULKAN_TEST "Build Vulkan Test Program" ON)
option(VR_VULKAN_USE_SYSTEM_LIB "Build Vulkan With System Library" OFF)
option(VR_VULKAN_VALIDATION "Enable Vulkan Validation" OFF)

set(TARGET nnr_vulkan)
set(VR_RUNTIME_HDR ${CMAKE_CURRENT_LIST_DIR}/../../../../../include)
set(VR_VULKAN_PUBLIC_HDR_DIR  ${CMAKE_CURRENT_LIST_DIR}/../../../../../../3rd_party/vulkan/include)
set(VR_VULKAN_VMA_HDR ${CMAKE_CURRENT_LIST_DIR}/../../../../../../3rd_party/vma/src)


file(GLOB_RECURSE VR_VULKAN_PUBLIC_HDRS  ${CMAKE_CURRENT_LIST_DIR}/../*.h ${CMAKE_CURRENT_LIST_DIR}/../*.hpp)
file(GLOB_RECURSE VR_VULKAN_SRCS ${CMAKE_CURRENT_LIST_DIR}/../*.cpp)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fvisibility=hidden")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")



add_library(${TARGET} STATIC ${VR_VULKAN_PUBLIC_HDRS} ${VR_VULKAN_SRCS})

# vma include dir
target_include_directories(${TARGET} PUBLIC ${VR_VULKAN_VMA_HDR})
# other include dirs
target_include_directories(${TARGET} PUBLIC  ${VR_RUNTIME_HDR})
target_include_directories(${TARGET} PUBLIC  ${CMAKE_CURRENT_LIST_DIR}/..)
target_include_directories(${TARGET} PUBLIC  ${VR_VULKAN_PUBLIC_HDR_DIR})



if(VR_VULKAN_USE_SYSTEM_LIB)
    find_package(Vulkan REQUIRED)
    if (APPLE AND NOT Vulkan_LIBRARIES) 
        message(STATUS "No Vulkan SDK was found, using prebuilt MoltenVK.")
        set(MOLTENVK_DIR "${CMAKE_CURRENT_LIST_DIR}/../../../../../../../3rd_party/moltenvk/macOS/libMoltenVK.dylib")
        target_link_libraries(nnrruntime dl ${MOLTENVK_DIR})
    else()
        target_link_libraries(nnrruntime dl ${Vulkan_LIBRARIES})
        message(STATUS "Find Vulkan from Vulkan_LIBRARIES: ${Vulkan_LIBRARIES}")
    endif()
else()
    add_definitions(-DVR_VULKAN_USE_LIB_WRAPPER)
endif()

if(VR_VULKAN_VALIDATION)
    add_definitions(-DVR_VULKAN_VALIDATION)
endif()

if(ANDROID)
    target_compile_definitions(${TARGET} PRIVATE VK_USE_PLATFORM_ANDROID_KHR)
elseif(WIN32)
    target_compile_definitions(${TARGET} PRIVATE VK_USE_PLATFORM_WIN32_KHR)
elseif(APPLE)
    target_compile_definitions(${TARGET} PRIVATE VK_USE_PLATFORM_METAL_EXT)
# elseif(UNIX)
#     # Choose WSI based on VKB_WSI_SELECTION
#     if (VKB_WSI_SELECTION STREQUAL XCB OR VKB_WSI_SELECTION STREQUAL XLIB OR VKB_WSI_SELECTION STREQUAL WAYLAND)
#         find_package(PkgConfig REQUIRED)
#     endif()
#     if (VKB_WSI_SELECTION STREQUAL XCB)
#         pkg_check_modules(XCB xcb REQUIRED)
#         if (XCB_FOUND)
#             target_compile_definitions(vulkan INTERFACE VK_USE_PLATFORM_XCB_KHR)
#         endif()
#     elseif (VKB_WSI_SELECTION STREQUAL XLIB)
#         pkg_check_modules(X11 x11 REQUIRED)
#         if (X11_FOUND)
#             target_compile_definitions(vulkan INTERFACE VK_USE_PLATFORM_XLIB_KHR)
#         endif()
#     elseif (VKB_WSI_SELECTION STREQUAL WAYLAND)
#         pkg_check_modules(WAYLAND wayland-client REQUIRED)
#         if (WAYLAND_FOUND)
#             target_compile_definitions(vulkan INTERFACE VK_USE_PLATFORM_WAYLAND_KHR)
#         endif()
#     elseif (VKB_WSI_SELECTION STREQUAL D2D)
#         set(DIRECT_TO_DISPLAY TRUE)
#         set(DIRECT_TO_DISPLAY TRUE PARENT_SCOPE)
#         target_compile_definitions(vulkan INTERFACE VK_USE_PLATFORM_DISPLAY_KHR)
#     else()
#         message(FATAL_ERROR "Unknown WSI")
#     endif()    
else()
    message(FATAL_ERROR "Platform not supported. Only supports Windows, Android, IOS and MacOS")    
endif()

if (VR_BUILD_VULKAN_TEST)
    if (APPLE) 
        add_executable(vulkan_test test.cpp)
        target_link_libraries(vulkan_test PRIVATE ${TARGET} dl)
    endif()
endif()

# install(TARGETS ${TARGET} ARCHIVE DESTINATION lib/${DIST_DIR})
