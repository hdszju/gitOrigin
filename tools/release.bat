@echo off
cmake >nul 2>nul
if errorlevel 1 (echo release require 'cmake' but nothing is found && pause && exit)
7z >nul 2>nul
if errorlevel 1 (echo release require '7z' but nothing is found && pause && exit)


for /f "tokens=2 delims==" %%a in ('wmic path win32_operatingsystem get LocalDateTime /value') do (  
    set t=%%a  
) 
set release_date=%t:~0,12%
echo RELEASE champion OF %release_date%
set "targetFile=champion_%release_date%"
set "route=%~dp0"
set "dependgrpc=grpc-0_11_1"
set "dependpb=protobuf-3.0.0-beta-1"


if exist %route%..\build rd /s /q %route%..\build
md %route%..\build
cd %route%..\build
cmake ..\ -G "Visual Studio 12 2013"
echo Start building Release version...
cmake --build . --config Release -- /fileLogger /consoleloggerparameters:Summary;ErrorsOnly /verbosity:minimal /maxcpucount
echo Start building Debug version...
cmake --build . --config Debug -- /fileLogger /consoleloggerparameters:Summary;ErrorsOnly /verbosity:minimal /maxcpucount


echo Start patching champion files...
if exist %route%..\%targetFile% (rd %route%..\%targetFile% /s /q)
md %route%..\%targetFile%
cd %route%..\%targetFile%
md lib lib\Release lib\Debug
md bin
md include
md py27_virtualenv
md node node\node_modules node\node_modules\grpc

cd %route%..\build\Release
copy champion_master_exe.exe %route%..\%targetFile%\bin\
copy libeay32.dll %route%..\%targetFile%\bin\
copy ssleay32.dll %route%..\%targetFile%\bin\
copy zlib.dll %route%..\%targetFile%\bin\
cd %route%..\build\Release
copy champion.lib %route%..\%targetFile%\lib\Release\
cd %route%..\build\Debug
copy champion.lib %route%..\%targetFile%\lib\Debug\

cd %route%..\build
copy champion_rpc.pb.h %route%..\%targetFile%\include\
copy champion_rpc.grpc.pb.h %route%..\%targetFile%\include\
cd %route%..\src\champion
copy *.h %route%..\%targetFile%\include\


echo Start patching champion dependent files (grpc)...
cd %route%..\external_libs\%dependgrpc%
xcopy include %route%..\%targetFile%\include\ /e /q
xcopy lib\Release %route%..\%targetFile%\lib\Release\ /e /q
xcopy lib\Debug %route%..\%targetFile%\lib\Debug\ /e /q


echo Start patching champion dependent files (protobuf)...
cd %route%..\external_libs\%dependpb%
copy bin\protoc.exe %route%..\%targetFile%\bin\
copy lib\Release\libprotobuf.lib %route%..\%targetFile%\lib\Release\
copy lib\Debug\libprotobuf.lib %route%..\%targetFile%\lib\Debug\
xcopy include %route%..\%targetFile%\include\ /e /q


echo Start patching champion python and dependencies...
cd %route%..\external_libs\%dependgrpc%
xcopy py27_virtualenv %route%..\%targetFile%\py27_virtualenv\ /e /q
cd %route%..\%targetFile%
md py27_virtualenv\Lib\site-packages\champion
cd %route%..\src\champion_python
echo | call run_proto_codegen.bat
::%route%..\external_libs\%dependgrpc%\py27_virtualenv\Scripts\python -m compileall .
::copy *.pyc %route%..\%targetFile%\py27_virtualenv\Lib\site-packages\champion\
copy *.py %route%..\%targetFile%\py27_virtualenv\Lib\site-packages\champion\


echo Start patching champion node and dependencies...
cd %route%..\src\champion_node
if exist %route%..\src\champion_node\node_modules rd /s /q %route%..\src\champion_node\node_modules
call npm install
call npm link %route%..\external_libs\%dependgrpc%\node
xcopy node_modules %route%..\%targetFile%\node\node_modules\ /e /q
copy *.js %route%..\%targetFile%\node\
copy package.json %route%..\%targetFile%\node\
copy README.MD %route%..\%targetFile%\node\
copy node_test.proto %route%..\%targetFile%\node\
copy ..\champion\champion_rpc.proto %route%..\%targetFile%\node\


echo Start patching to an archive file...
cd %route%..
if exist %route%..\%targetFile%.zip (del %route%..\%targetFile%.zip)
7z a %targetFile%.zip %route%..\%targetFile% >nul
rd %route%..\%targetFile% /s /q


echo All works are done
pause



