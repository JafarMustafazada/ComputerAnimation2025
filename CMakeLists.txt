cmake_minimum_required(VERSION 3.20)
project(oglproj1)
set(CMAKE_CXX_STANDARD 20)
### _________________________________________________________________________________________________________


# Properties #
set(OUTPUT_BASE_DIR ${CMAKE_BINARY_DIR})

## For single-configuration generators (e.g. Makefile, Ninja, GCC)
if(NOT CMAKE_CONFIGURATION_TYPES)
set(OUTPUT_BASE_DIR ${OUTPUT_BASE_DIR}/bin)
endif()

## If you want to just set the output directory to a fixed path, uncomment below
# set(OUTPUT_BASE_DIR $<1:${CMAKE_SOURCE_DIR}/bin>)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${OUTPUT_BASE_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${OUTPUT_BASE_DIR})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${OUTPUT_BASE_DIR})

## GLFW options
set(GLFW_BUILD_EXAMPLES   OFF CACHE BOOL "Build the GLFW example programs" FORCE)
set(GLFW_BUILD_TESTS      OFF CACHE BOOL "Build the GLFW test programs" FORCE)
set(GLFW_BUILD_DOCS       OFF CACHE BOOL "Build the GLFW documentation" FORCE)
if(CMAKE_HOST_UNIX AND NOT APPLE)
	set(GLFW_BUILD_WAYLAND OFF CACHE BOOL "Use Wayland display server" FORCE)
	set(GLFW_BUILD_X11 ON CACHE BOOL "Use X11 display server" FORCE)
elseif(APPLE)
	## macOS: Use Cocoa (default), X11 only if XQuartz is available
	find_path(X11_INCLUDE_DIR X11/Xlib.h)
	if(X11_INCLUDE_DIR)
		message(STATUS "XQuartz detected, enabling X11 support in GLFW")
		set(GLFW_USE_X11 ON CACHE BOOL "Use X11 display server" FORCE)
	else()
		set(GLFW_USE_X11 OFF CACHE BOOL "Use X11 display server" FORCE)
	endif()    
elseif(WIN32)
	## No extra setup needed
endif()
### _________________________________________________________________________________________________________


# Add the subdirectories #
add_subdirectory(src/glfw1)
add_subdirectory(src/glad1)
### _________________________________________________________________________________________________________


# Create the executable #
# file(GLOB_RECURSE MY_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
set(MY_SOURCES
	# src/main1.cpp # For project 1
	# src/main2.cpp # For project 2
	# src/main3.cpp # For project 3
	# src/main4.cpp # For project 4
	src/main5.cpp # For project 5
)
add_executable(oglproj1 ${MY_SOURCES})
### _________________________________________________________________________________________________________


# Links to the executable #
find_package(OpenGL REQUIRED)
target_link_libraries(oglproj1 PRIVATE glfw OpenGL::GL)
target_link_libraries(oglproj1 PRIVATE glad1)
### _________________________________________________________________________________________________________


# Set includde directories #
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/include")
target_include_directories(oglproj1 PRIVATE ${CMAKE_SOURCE_DIR}/src/glfw1/include)
target_include_directories(oglproj1 PRIVATE ${CMAKE_SOURCE_DIR}/src/glad1/include)
### _________________________________________________________________________________________________________