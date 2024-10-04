@echo off
setlocal
cd /D "%~dp0"

:: if not exist build (mkdir build)

set compilation_flags=-std=c++17 -O0 -g -lUser32 -lgdi32 -lvulkan-1

clang++ %compilation_flags% -o breakout.exe code\breakout.cpp
