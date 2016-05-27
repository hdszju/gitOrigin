cmake >nul 2>nul
if errorlevel 1 (echo release require 'cmake' but nothing is found && pause && exit)
set "route=%~dp0"

RMDIR %route%..\build /s /q
MKDIR %route%..\build
cd %route%..\build
cmake ..\ -G "Visual Studio 12 2013"

echo "build projects"
cmake --build . --config Release -- /fileLogger /consoleloggerparameters:Summary;ErrorsOnly /verbosity:minimal /maxcpucount

pause
