@echo off
setlocal
cd /D "%~dp0"

:: if not exist build (mkdir build)

set compilation_flags=-lUser32 -lgdi32 -lvulkan-1 -Wno-c++20-extensions -std=c++17 -O0 -g

clang++ %compilation_flags% -o breakout.exe code\breakout.cpp

:: build shaders

glslc code\basic.frag -o data\basic.frag.spv
glslc code\basic.vert -o data\basic.vert.spv
