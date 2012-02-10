@echo off
REM ///////////////////////////////////////////////////////////////////////////
REM // Set Paths
REM ///////////////////////////////////////////////////////////////////////////
set "MSVC_PATH=C:\Program Files\Microsoft Visual Studio 10.0\VC"
set "NSIS_PATH=C:\Program Files\NSIS\Unicode"
set "QTVC_PATH=C:\MuldeR\Qt\4.8.0"
set "UPX3_PATH=C:\Program Files\UPX"

REM ###############################################
REM # DO NOT MODIFY ANY LINES BELOW THIS LINE !!! #
REM ###############################################


REM ///////////////////////////////////////////////////////////////////////////
REM // Setup environment
REM ///////////////////////////////////////////////////////////////////////////
if exist "%QTVC_PATH%\bin\qtvars.bat" ( call "%QTVC_PATH%\bin\qtvars.bat" )
if exist "%QTVC_PATH%\bin\qtenv2.bat" ( call "%QTVC_PATH%\bin\qtenv2.bat" )
call "%MSVC_PATH%\vcvarsall.bat" x86

REM ///////////////////////////////////////////////////////////////////////////
REM // Check environment
REM ///////////////////////////////////////////////////////////////////////////
if "%VCINSTALLDIR%"=="" (
	echo %%VCINSTALLDIR%% not specified. Please check your MSVC_PATH var!
	goto BuildError
)
if "%QTDIR%"=="" (
	echo %%QTDIR%% not specified. Please check your MSVC_PATH var!
	goto BuildError
)
if not exist "%VCINSTALLDIR%\bin\cl.exe" (
	echo C++ compiler not found. Please check your MSVC_PATH var!
	goto BuildError
)
if not exist "%QTDIR%\bin\moc.exe" (
	echo Qt meta compiler not found. Please check your QTVC_PATH var!
	goto BuildError
)

REM ///////////////////////////////////////////////////////////////////////////
REM // Build the binaries
REM ///////////////////////////////////////////////////////////////////////////
echo ---------------------------------------------------------------------
echo BEGIN BUILD
echo ---------------------------------------------------------------------
MSBuild.exe /property:Configuration=release /target:clean "%~dp0\x264_launcher.sln"
if not "%ERRORLEVEL%"=="0" goto BuildError
MSBuild.exe /property:Configuration=release /target:rebuild "%~dp0\x264_launcher.sln"
if not "%ERRORLEVEL%"=="0" goto BuildError

REM ///////////////////////////////////////////////////////////////////////////
REM // Copy base files
REM ///////////////////////////////////////////////////////////////////////////
echo ---------------------------------------------------------------------
echo BEGIN PACKAGING
echo ---------------------------------------------------------------------
set "PACK_PATH=%TMP%\~%RANDOM%%RANDOM%.tmp"
mkdir "%PACK_PATH%"
mkdir "%PACK_PATH%\imageformats"
mkdir "%PACK_PATH%\toolset"
copy "%~dp0\bin\Release\*.exe" "%PACK_PATH%"
copy "%~dp0\bin\Release\toolset\*.exe" "%PACK_PATH%\toolset"
copy "%~dp0\*.txt" "%PACK_PATH%"

REM ///////////////////////////////////////////////////////////////////////////
REM // Copy dependencies
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
REM // Compress
REM ///////////////////////////////////////////////////////////////////////////
"%UPX3_PATH%\upx.exe" --brute "%PACK_PATH%\*.exe"
"%UPX3_PATH%\upx.exe" --best "%PACK_PATH%\*.dll"

REM ///////////////////////////////////////////////////////////////////////////
REM // Get current date (in ISO format)
REM ///////////////////////////////////////////////////////////////////////////
if not exist "%~dp0\etc\date.exe" BuildError
for /F "tokens=1,2 delims=:" %%a in ('"%~dp0\etc\date.exe" +ISODATE:%%Y-%%m-%%d') do (
	if "%%a"=="ISODATE" set "ISO_DATE=%%b"
)
if "%ISO_DATE%"=="" BuildError

REM ///////////////////////////////////////////////////////////////////////////
REM // Setup install parameters
REM ///////////////////////////////////////////////////////////////////////////
set "NSI_FILE=%TMP%\~%RANDOM%%RANDOM%.nsi"
set "OUT_NAME=x264_x64.%ISO_DATE%"
set "OUT_PATH=%~dp0\bin"
set "OUT_FULL=%OUT_PATH%\%OUT_NAME%.exe"
:GenerateOutfileName
if exist "%OUT_FULL%" (
	set "OUT_NAME=%OUT_NAME%.new"
	set "OUT_FULL=%OUT_PATH%\%OUT_NAME%.exe"
	goto GenerateOutfileName
)

REM ///////////////////////////////////////////////////////////////////////////
REM // Generate install script
REM ///////////////////////////////////////////////////////////////////////////
echo #Generated File - Do NOT modify! > "%NSI_FILE%"
echo !define ZIP2EXE_NAME `Simple x264 Launcher (%ISO_DATE%)` >> "%NSI_FILE%"
echo !define ZIP2EXE_OUTFILE `%OUT_FULL%` >> "%NSI_FILE%"
echo !define ZIP2EXE_COMPRESSOR_LZMA >> "%NSI_FILE%"
echo !define ZIP2EXE_INSTALLDIR `$PROGRAMFILES\MuldeR\Simple x264 Launcher v2` >> "%NSI_FILE%"
echo !define MUI_INSTFILESPAGE_COLORS "C5DEFB 000000" >> "%NSI_FILE%"
echo RequestExecutionLevel Admin >> "%NSI_FILE%"
echo ShowInstDetails show >> "%NSI_FILE%"
echo !include `${NSISDIR}\Contrib\zip2exe\Base.nsh` >> "%NSI_FILE%"
echo !include `${NSISDIR}\Contrib\zip2exe\Modern.nsh` >> "%NSI_FILE%"
echo !insertmacro SECTION_BEGIN >> "%NSI_FILE%"
echo File /r `%PACK_PATH%\*.*` >> "%NSI_FILE%"
echo !include `%~dp0\etc\shortcut.nsh` >> "%NSI_FILE%"
echo !insertmacro SECTION_END >> "%NSI_FILE%"
echo !include `%~dp0\etc\check_os.nsh` >> "%NSI_FILE%"
echo !include `%~dp0\etc\finalization.nsh` >> "%NSI_FILE%"
echo !include `%~dp0\etc\version.nsh` >> "%NSI_FILE%"
echo !insertmacro X264_VERSIONINFO `%ISO_DATE%` >> "%NSI_FILE%"

REM ///////////////////////////////////////////////////////////////////////////
REM // Build the installer
REM ///////////////////////////////////////////////////////////////////////////
"%NSIS_PATH%\makensis.exe" "%NSI_FILE%"
if not "%ERRORLEVEL%"=="0" goto BuildError
attrib +R "%OUT_FULL%"
del "%NSI_FILE%"
rmdir /Q /S "%PACK_PATH%"

REM ///////////////////////////////////////////////////////////////////////////
REM // COMPLETE
REM ///////////////////////////////////////////////////////////////////////////
echo.
echo Build completed.
echo.
pause
goto:eof

REM ///////////////////////////////////////////////////////////////////////////
REM // FAILED
REM ///////////////////////////////////////////////////////////////////////////
:BuildError
echo.
echo Build has failed !!!
echo.
pause
