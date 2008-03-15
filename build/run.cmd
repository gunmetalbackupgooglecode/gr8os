@echo off
echo.
echo Starting system
echo.
if not exist ~Debug\floppy.ima goto error_1
bochs -q -f gr8os.bxrc
exit 0
:error_1
echo Floppy image not exist. Stop.
pause
exit 1
