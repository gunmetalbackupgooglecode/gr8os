@echo off

echo Creating directory (if not exists)
md image

echo Clearing directory
cd image
echo y | del *.*
cd ..

echo Copying files
copy loader\bootcode.bin image
copy loader\grldr.bin image
copy kernel.exe image

echo Creating image
makefd.exe image

echo Image created

pause