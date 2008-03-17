@echo off
echo.
echo Creating floppy image
if exist ~Debug\floppy.ima del ~Debug\floppy.ima
if exist ~Temp\kernel.exe move ~Temp\kernel.exe ~Debug\kernel.exe
cd ~Debug
..\tools\makefd.exe .
cd ..
if exist ~Debug\floppy.ima goto end
echo.
echo Error.
pause
:end
echo.
exit 0