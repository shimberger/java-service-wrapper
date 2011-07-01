@echo off
setlocal

set BUILDFILE=%~dp0%build.xml
echo --------------------
echo Wrapper Build System
echo using %BUILDFILE%
echo --------------------

call "%ANT_HOME%\bin\ant.bat" -f "%BUILDFILE%"  -Dbits=32 %1 %2 %3 %4 %5 %6 %7 %8

