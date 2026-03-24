@echo off
setlocal

echo ===== Build WindowsAutoShutdown =====

if not exist build mkdir build

set CXX=g++
set RC=windres
set CXXFLAGS=-std=c++17 -DUNICODE -D_UNICODE -Wall -Wextra -finput-charset=UTF-8 -fexec-charset=UTF-8
set LDFLAGS=-municode -mwindows -lcomctl32 -lshell32 -lole32 -loleaut32 -luuid -ldwmapi -luxtheme

%CXX% -c src\main.cpp %CXXFLAGS% -o build\main.o
if errorlevel 1 goto error

%CXX% -c src\MainWindow.cpp %CXXFLAGS% -o build\MainWindow.o
if errorlevel 1 goto error

%CXX% -c src\Config.cpp %CXXFLAGS% -o build\Config.o
if errorlevel 1 goto error

%CXX% -c src\ShutdownScheduler.cpp %CXXFLAGS% -o build\ShutdownScheduler.o
if errorlevel 1 goto error

%CXX% -c src\FolderMonitor.cpp %CXXFLAGS% -o build\FolderMonitor.o
if errorlevel 1 goto error

%CXX% -c src\ReminderWindow.cpp %CXXFLAGS% -o build\ReminderWindow.o
if errorlevel 1 goto error

%RC% --codepage=65001 res\resource.rc -O coff -o build\resource.o
if errorlevel 1 goto error

echo Linking...
%CXX% build\main.o build\MainWindow.o build\Config.o build\ShutdownScheduler.o build\FolderMonitor.o build\ReminderWindow.o build\resource.o -o build\WindowsAutoShutdown.exe %LDFLAGS%
if errorlevel 1 goto error

echo ===== Build succeeded =====
echo Output: build\WindowsAutoShutdown.exe
goto end

:error
echo ===== Build failed =====
exit /b 1

:end
endlocal

