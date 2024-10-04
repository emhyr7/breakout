@echo off
setlocal
cd /D "%~dp0"

set CFLAGS=-std=c++17 -O0 -g

clang++ %CFLAGS% -lUser32 -lgdi32 -lvulkan-1 -o breakout.exe code\breakout.cpp code\breakout_commons.cpp
