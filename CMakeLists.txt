cmake_minimum_required(VERSION 3.15)
project(Downloader CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(Downloader WIN32 main.cpp)
target_link_libraries(Downloader PRIVATE user32 shell32 ole32 shlwapi comctl32 gdi32 advapi32)
