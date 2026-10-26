@echo off
rem Builds OpenClaw.exe for S60 3rd Edition (ARMV5 urel, RVCT 2.2, the SDK in C:\epoc32 - the one Carbide uses).
rem Run from cmd or PowerShell, not from git-bash (MSYS breaks the "cmd /c" calls of the Symbian tools).
rem     build.bat          incremental
rem     build.bat clean    rebuild from scratch
setlocal
set PATH=C:\ARM\RVCT\Programs\2.2\349\win_32-pentium;C:\epoc32\tools;C:\epoc32\gcc\bin;%PATH%
set EPOCROOT=\
rem makmake resolves nested "../x.h" includes against the wrong folder: no header dependencies
rem (after changing a header, build.bat clean)
set SYMBIANBUILD_DEPENDENCYOFF=1
cd /d "%~dp0group"
call bldmake bldfiles || goto error
if /i "%1"=="clean" call perl -S ABLD.PL "%~p0group\\" reallyclean armv5 urel
rem (abld build doesn't always regenerate the makefiles after editing a .mmp)
call perl -S ABLD.PL "%~p0group\\" makefile armv5 || goto error
rem (only the urel steps: "build" also makes the udeb libraries, and RVCT 2.2 crashes on one game file at -O0)
call perl -S ABLD.PL "%~p0group\\" resource armv5 urel > "%TEMP%\openclaw_abld.log" 2>&1
call perl -S ABLD.PL "%~p0group\\" target armv5 urel >> "%TEMP%\openclaw_abld.log" 2>&1
call perl -S ABLD.PL "%~p0group\\" final armv5 urel >> "%TEMP%\openclaw_abld.log" 2>&1
type "%TEMP%\openclaw_abld.log"
findstr /c:"Error:" /c:"ERROR:" /c:"make: ***" "%TEMP%\openclaw_abld.log" > nul && goto error
echo.
echo OK: C:\epoc32\release\armv5\urel\OpenClaw.exe
exit /b 0
:error
echo.
echo BUILD FAILED
exit /b 1
