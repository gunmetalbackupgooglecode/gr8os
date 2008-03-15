@echo off
echo.
echo Compiling Loader
if exist ~Debug\bootcode.bin del ~Debug\bootcode.bin
fasm ..\loader\bootcode.asm ~Debug\bootcode.bin
if not exist ~Debug\bootcode.bin goto error_1
goto end
:error_1
echo.
echo Error.
pause

:end
echo.