@echo off
REM ///////////////////////////////////////////////////////////////////////////
set "MSVC_PATH=D:\Microsoft Visual Studio 10.0\VC"
set "NSIS_PATH=E:\NSIS\_Unicode"
set "QTVC_PATH=E:\QtSDK\Desktop\Qt\4.8.0\msvc2010"
set "UPX3_PATH=E:\UPX"
REM ///////////////////////////////////////////////////////////////////////////
if exist "%QTVC_PATH%\bin\qtvars.bat" ( call "%QTVC_PATH%\bin\qtvars.bat" )
if exist "%QTVC_PATH%\bin\qtenv2.bat" ( call "%QTVC_PATH%\bin\qtenv2.bat" )
call "%MSVC_PATH%\vcvarsall.bat" x86
echo ---------------------------------------------------------------------
echo BEGIN BUILD
echo ---------------------------------------------------------------------
MSBuild.exe /property:Configuration=release /target:clean "%~dp0\x264_launcher.sln"
if not "%ERRORLEVEL%"=="0" goto BuildError
MSBuild.exe /property:Configuration=release /target:rebuild "%~dp0\x264_launcher.sln"
if not "%ERRORLEVEL%"=="0" goto BuildError
echo ---------------------------------------------------------------------
echo BEGIN PACKAGING
echo ---------------------------------------------------------------------
set "PACK_PATH=%TMP%\~%RANDOM%%RANDOM%.tmp"
mkdir "%PACK_PATH%"
mkdir "%PACK_PATH%\imageformats"
mkdir "%PACK_PATH%\toolset"
copy "%~dp0\bin\Release\*.exe" "%PACK_PATH%"
copy "%~dp0\bin\Release\toolset\*.exe" "%PACK_PATH%\toolset"
REM ///////////////////////////////////////////////////////////////////////////
copy "%MSVC_PATH%\redist\x86\Microsoft.VC100.CRT\*.dll" "%PACK_PATH%"
copy "%QTVC_PATH%\bin\QtCore4.dll" "%PACK_PATH%"
copy "%QTVC_PATH%\bin\QtGui4.dll" "%PACK_PATH%"
copy "%QTVC_PATH%\bin\QtSvg4.dll" "%PACK_PATH%"
copy "%QTVC_PATH%\bin\QtXml4.dll" "%PACK_PATH%"
copy "%QTVC_PATH%\bin\QtXml4.dll" "%PACK_PATH%"
copy "%QTVC_PATH%\plugins\imageformats\*.dll" "%PACK_PATH%\imageformats"
del "%PACK_PATH%\imageformats\*d4.dll"
REM ///////////////////////////////////////////////////////////////////////////
"%UPX3_PATH%\upx.exe" --brute "%PACK_PATH%\*.exe"
"%UPX3_PATH%\upx.exe" --best "%PACK_PATH%\*.dll"
REM ///////////////////////////////////////////////////////////////////////////
if not exist "%~dp0\etc\date.exe" BuildError
for /F "tokens=1,2 delims=:" %%a in ('"%~dp0\etc\date.exe" +ISODATE:%%Y-%%m-%%d') do (
	if "%%a"=="ISODATE" set "ISO_DATE=%%b"
)
if "%ISO_DATE%"=="" BuildError
REM ///////////////////////////////////////////////////////////////////////////
set "NSIS_FILE=%TMP%\~%RANDOM%%RANDOM%.nsi"
echo !define ZIP2EXE_NAME `Simple x264 Launcher (%ISO_DATE%)` > "%NSIS_FILE%"
echo !define ZIP2EXE_OUTFILE `%~dp0\bin\x264_x64.%ISO_DATE%.exe` >> "%NSIS_FILE%"
echo !define ZIP2EXE_COMPRESSOR_LZMA >> "%NSIS_FILE%"
echo !define ZIP2EXE_INSTALLDIR `$PROGRAMFILES\MuldeR\Simple x264 Launcher v2` >> "%NSIS_FILE%"
echo !include `${NSISDIR}\Contrib\zip2exe\Base.nsh` >> "%NSIS_FILE%"
echo !include `${NSISDIR}\Contrib\zip2exe\Modern.nsh` >> "%NSIS_FILE%"
echo !insertmacro SECTION_BEGIN >> "%NSIS_FILE%"
echo File /r `%PACK_PATH%\*.*` >> "%NSIS_FILE%"
echo !insertmacro SECTION_END >> "%NSIS_FILE%"
"%NSIS_PATH%\makensis.exe" "%NSIS_FILE%"
if not "%ERRORLEVEL%"=="0" goto BuildError
del "%NSIS_FILE%"
rmdir /Q /S "%PACK_PATH%"
REM ///////////////////////////////////////////////////////////////////////////
echo.
echo Build completed.
echo.
pause
goto:eof
REM ///////////////////////////////////////////////////////////////////////////
:BuildError
echo.
echo Build has failed !!!
echo.
pause
