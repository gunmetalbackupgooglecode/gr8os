@echo off
cd build
echo.
echo REBUILD :: DEBUG
echo.
start /B /W makeboot.cmd -debug
if errorlevel 0 goto step_1
echo Stop.
exit 1
:step_1
start /B  /W makeimage.cmd -debug
if errorlevel 0 goto finish
echo Stop.
exit 1
:finish
echo.
echo Rebuild successed
echo.
exit 0

