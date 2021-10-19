cmake_minimum_required(VERSION 3.14)

project(
	ConsoleImages VERSION 1.0
	DESCRIPTION "Images in cmd.exe, spooky"
	LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

add_executable(${PROJECT_NAME} "")

add_subdirectory(src)

target_link_libraries(${PROJECT_NAME} gdi32)

