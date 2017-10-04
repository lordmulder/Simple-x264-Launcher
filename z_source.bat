@echo off
set "GIT_PATH=c:\Program Files\Git"
set "PATH=%GIT_PATH%;%GIT_PATH%\mingw64\bin;%GIT_PATH%\usr\bin;%PATH%"
set "OUT_PATH=%TEMP%\~%RANDOM%%RANDOM%.tmp"

mkdir "%OUT_PATH%"
for %%i in (x264_x64_launcher,MUtilities,Prerequisites) do (
	mkdir "%OUT_PATH%\%%i"
)

call::git_export "%~dp0"               x264_x64_launcher
call::git_export "%~dp0\..\MUtilities" MUtilities

copy "%~dp0\Copying.txt" "%OUT_PATH%"

mkdir "%OUT_PATH%\Prerequisites"
echo Please extract the Prerequisites files here! > "%OUT_PATH%\Prerequisites\README_1ST.txt"

pushd "%OUT_PATH%"
tar -cvf ./sources.tar *
"%~dp0\..\Prerequisites\SevenZip\7za.exe" a -txz "%~dp0\~sources.tar.xz" "sources.tar"
popd

pushd "%~dp0"
rmdir /S /Q "%OUT_PATH%"

pause
exit


:git_export
pushd "%~1"
git archive --verbose --output "%OUT_PATH%\%~2.tar" MASTER
popd
pushd "%OUT_PATH%\%~2"
tar -xvf "../%~2.tar"
del "%OUT_PATH%\%~2.tar"
popd
goto:eof
