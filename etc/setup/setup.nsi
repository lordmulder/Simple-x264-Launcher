; ///////////////////////////////////////////////////////////////////////////////
; // Simple x264 Launcher
; // Copyright (C) 2004-2018 LoRd_MuldeR <MuldeR2@GMX.de>
; //
; // This program is free software; you can redistribute it and/or modify
; // it under the terms of the GNU General Public License as published by
; // the Free Software Foundation; either version 2 of the License, or
; // (at your option) any later version.
; //
; // This program is distributed in the hope that it will be useful,
; // but WITHOUT ANY WARRANTY; without even the implied warranty of
; // MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
; // GNU General Public License for more details.
; //
; // You should have received a copy of the GNU General Public License along
; // with this program; if not, write to the Free Software Foundation, Inc.,
; // 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
; //
; // http://www.gnu.org/licenses/gpl-2.0.txt
; ///////////////////////////////////////////////////////////////////////////////


;--------------------------------
;Basic Defines
;--------------------------------

!ifndef NSIS_UNICODE
  !error "NSIS_UNICODE is undefined, please compile with Unicode NSIS !!!"
!endif

!ifndef X264_BUILD
  !error "X264_BUILD is not defined !!!"
!endif
!ifndef X264_DATE
  !error "X264_DATE is not defined !!!"
!endif
!ifndef X264_OUTPUT_FILE
  !error "X264_OUTPUT_FILE is not defined !!!"
!endif
!ifndef X264_SOURCE_PATH
  !error "X264_SOURCE_PATH is not defined !!!"
!endif

;UUID
!define MyRegPath "SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\{986E454F-DACA-4326-A9C7-3E46C0BFFDCE}"

;Web-Site
!define MyWebSite "http://muldersoft.com/"

;App Paths
!define AppPaths "SOFTWARE\Microsoft\Windows\CurrentVersion\App Paths"


;--------------------------------
;Manifest
;--------------------------------

!tempfile PACKHDRTEMP
!packhdr "${PACKHDRTEMP}" '"..\..\..\Prerequisites\MSVC\redist\bin\mt.exe" -manifest "setup.manifest" -outputresource:"${PACKHDRTEMP};1" && "..\..\..\Prerequisites\UPX\upx.exe" --brute "${PACKHDRTEMP}"'


;--------------------------------
;Includes
;--------------------------------

!include `MUI2.nsh`
!include `WinVer.nsh`
!include `x64.nsh`
!include `StdUtils.nsh`


;--------------------------------
;Installer Attributes
;--------------------------------

RequestExecutionLevel admin
ShowInstDetails show
ShowUninstDetails show
Name "Simple x264/x265 Launcher [Build #${X264_BUILD}]"
OutFile "${X264_OUTPUT_FILE}"
BrandingText "Date created: ${X264_DATE} [Build #${X264_BUILD}]"
InstallDir "$PROGRAMFILES\MuldeR\Simple x264 Launcher v2"
InstallDirRegKey HKLM "${MyRegPath}" "InstallLocation"


;--------------------------------
;Compressor
;--------------------------------

SetCompressor /SOLID LZMA
SetCompressorDictSize 64


;--------------------------------
;Reserved Files
;--------------------------------

ReserveFile "${NSISDIR}\Plugins\Aero.dll"
ReserveFile "${NSISDIR}\Plugins\LangDLL.dll"
ReserveFile "${NSISDIR}\Plugins\LockedList.dll"
ReserveFile "${NSISDIR}\Plugins\LockedList64.dll"
ReserveFile "${NSISDIR}\Plugins\nsDialogs.dll"
ReserveFile "${NSISDIR}\Plugins\nsExec.dll"
ReserveFile "${NSISDIR}\Plugins\StartMenu.dll"
ReserveFile "${NSISDIR}\Plugins\StdUtils.dll"
ReserveFile "${NSISDIR}\Plugins\System.dll"
ReserveFile "${NSISDIR}\Plugins\UserInfo.dll"


;--------------------------------
;Variables
;--------------------------------

Var StartMenuFolder
Var UpdateNotificationShown


;--------------------------------
;Version Info
;--------------------------------

!searchreplace PRODUCT_VERSION_DATE "${X264_DATE}" "-" "."
VIProductVersion "${PRODUCT_VERSION_DATE}.${X264_BUILD}"

VIAddVersionKey "Author" "LoRd_MuldeR <mulder2@gmx.de>"
VIAddVersionKey "Comments" "This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation; either version 2 of the License, or (at your option) any later version."
VIAddVersionKey "CompanyName" "Free Software Foundation"
VIAddVersionKey "FileDescription" "Simple x264 Launcher [Build #${X264_BUILD}]"
VIAddVersionKey "FileVersion" "${PRODUCT_VERSION_DATE}.${X264_BUILD}"
VIAddVersionKey "LegalCopyright" "Copyright 2004-2018 LoRd_MuldeR"
VIAddVersionKey "LegalTrademarks" "GNU"
VIAddVersionKey "OriginalFilename" "x264_x64.${X264_DATE}.exe"
VIAddVersionKey "ProductName" "Simple x264 Launcher"
VIAddVersionKey "ProductVersion" "Build #${X264_BUILD} (${X264_DATE})"
VIAddVersionKey "Website" "${MyWebSite}"


;--------------------------------
;MUI2 Interface Settings
;--------------------------------

!define MUI_ABORTWARNING
!define MUI_STARTMENUPAGE_REGISTRY_ROOT HKLM
!define MUI_STARTMENUPAGE_REGISTRY_KEY "${MyRegPath}"
!define MUI_STARTMENUPAGE_REGISTRY_VALUENAME "StartmenuFolder"
!define MUI_LANGDLL_REGISTRY_ROOT HKLM
!define MUI_LANGDLL_REGISTRY_KEY "${MyRegPath}"
!define MUI_LANGDLL_REGISTRY_VALUENAME "SetupLanguage"
!define MUI_STARTMENUPAGE_DEFAULTFOLDER "Simple x264 Launcher v2"
!define MUI_FINISHPAGE_NOAUTOCLOSE
!define MUI_UNFINISHPAGE_NOAUTOCLOSE
!define MUI_FINISHPAGE_RUN
!define MUI_FINISHPAGE_RUN_FUNCTION RunAppFunction
!define MUI_FINISHPAGE_SHOWREADME
!define MUI_FINISHPAGE_SHOWREADME_FUNCTION ShowReadmeFunction
!define MUI_FINISHPAGE_LINK ${MyWebSite}
!define MUI_FINISHPAGE_LINK_LOCATION ${MyWebSite}
!define MUI_ICON "${NSISDIR}\Contrib\Graphics\Icons\orange-install.ico"
!define MUI_UNICON "${NSISDIR}\Contrib\Graphics\Icons\orange-uninstall.ico"
!define MUI_WELCOMEFINISHPAGE_BITMAP "wizard.bmp"
!define MUI_UNWELCOMEFINISHPAGE_BITMAP "wizard-un.bmp"
!define MUI_HEADERIMAGE
!define MUI_HEADERIMAGE_BITMAP "header.bmp"
!define MUI_HEADERIMAGE_UNBITMAP "header-un.bmp"
!define MUI_LANGDLL_ALLLANGUAGES
!define MUI_CUSTOMFUNCTION_GUIINIT MyGuiInit
!define MUI_CUSTOMFUNCTION_UNGUIINIT un.MyGuiInit
!define MUI_LANGDLL_ALWAYSSHOW


;--------------------------------
;MUI2 Pages (Installer)
;--------------------------------

;Welcome
!define MUI_PAGE_CUSTOMFUNCTION_PRE SkipIfUnattended
!define MUI_WELCOMEPAGE_TITLE_3LINES
!define MUI_FINISHPAGE_TITLE_3LINES
!insertmacro MUI_PAGE_WELCOME

;License
!define MUI_PAGE_CUSTOMFUNCTION_PRE SkipIfUnattended
!insertmacro MUI_PAGE_LICENSE "license.rtf"

;Directory
!define MUI_PAGE_CUSTOMFUNCTION_PRE SkipIfUnattended
!define MUI_PAGE_CUSTOMFUNCTION_SHOW CheckForUpdate
!insertmacro MUI_PAGE_DIRECTORY

;Startmenu
!define MUI_PAGE_CUSTOMFUNCTION_PRE SkipIfUnattended
!insertmacro MUI_PAGE_STARTMENU Application $StartMenuFolder

;LockedList
Page Custom LockedListShow

;Install Files
!insertmacro MUI_PAGE_INSTFILES

;Finish
!define MUI_PAGE_CUSTOMFUNCTION_PRE SkipIfUnattended
!insertmacro MUI_PAGE_FINISH


;--------------------------------
;MUI2 Pages (Uninstaller)
;--------------------------------

;Welcome
!define MUI_WELCOMEPAGE_TITLE_3LINES
!define MUI_FINISHPAGE_TITLE_3LINES
!define MUI_PAGE_CUSTOMFUNCTION_PRE un.CheckForcedUninstall
!insertmacro MUI_UNPAGE_WELCOME

;Confirm
!define MUI_PAGE_CUSTOMFUNCTION_PRE un.CheckForcedUninstall
!insertmacro MUI_UNPAGE_CONFIRM

;LockedList
UninstPage Custom un.LockedListShow

;Uninstall
!insertmacro MUI_UNPAGE_INSTFILES

;Finish
!define MUI_PAGE_CUSTOMFUNCTION_PRE un.CheckForcedUninstall
!insertmacro MUI_UNPAGE_FINISH


;--------------------------------
;Languages
;--------------------------------
 
!insertmacro MUI_LANGUAGE "English" ;first language is the default language

; !insertmacro MUI_LANGUAGE "German"
; !insertmacro MUI_LANGUAGE "Spanish"
; !insertmacro MUI_LANGUAGE "Russian"
; !insertmacro MUI_LANGUAGE "Ukrainian"
; !insertmacro MUI_LANGUAGE "Polish"

; !insertmacro MUI_LANGUAGE "French"
; !insertmacro MUI_LANGUAGE "SpanishInternational"
; !insertmacro MUI_LANGUAGE "SimpChinese"
; !insertmacro MUI_LANGUAGE "TradChinese"
; !insertmacro MUI_LANGUAGE "Japanese"
; !insertmacro MUI_LANGUAGE "Italian"
; !insertmacro MUI_LANGUAGE "Dutch"
; !insertmacro MUI_LANGUAGE "Greek"
; !insertmacro MUI_LANGUAGE "Hungarian"
; !insertmacro MUI_LANGUAGE "Romanian"
; !insertmacro MUI_LANGUAGE "Serbian"
; !insertmacro MUI_LANGUAGE "SerbianLatin"
; !insertmacro MUI_LANGUAGE "Arabic"
; !insertmacro MUI_LANGUAGE "Portuguese"
; !insertmacro MUI_LANGUAGE "Afrikaans"
; !insertmacro MUI_LANGUAGE "Malay"
; !insertmacro MUI_LANGUAGE "Indonesian"


;--------------------------------
;Translation
;--------------------------------

;English
!include "language\en.nsh"


;--------------------------------
;LogicLib Extensions
;--------------------------------

!macro _UnattendedMode _a _b _t _f
	!insertmacro _LOGICLIB_TEMP
	${StdUtils.TestParameter} $_LOGICLIB_TEMP "Update"
	StrCmp "$_LOGICLIB_TEMP" "true" `${_t}` `${_f}`
!macroend
!define UnattendedMode `"" UnattendedMode ""`

!macro _ForcedMode _a _b _t _f
	!insertmacro _LOGICLIB_TEMP
	${StdUtils.TestParameter} $_LOGICLIB_TEMP "Force"
	StrCmp "$_LOGICLIB_TEMP" "true" `${_t}` `${_f}`
!macroend
!define ForcedMode `"" ForcedMode ""`

!macro _ValidFileName _a _b _t _f
	!insertmacro _LOGICLIB_TEMP
	${StdUtils.ValidFileName} $_LOGICLIB_TEMP `${_b}`
	StrCmp "$_LOGICLIB_TEMP" "ok" `${_t}` `${_f}`
!macroend
!define ValidFileName `"" ValidFileName`


;--------------------------------
;Installer initialization
;--------------------------------

Function .onInit
	InitPluginsDir

	; --------

	System::Call 'kernel32::CreateMutexA(i 0, i 0, t "{2B3D1EBF-B3B6-4E93-92B9-6853029A7162}") i .r1 ?e'
	Pop $0
	${If} $0 <> 0
		MessageBox MB_ICONSTOP|MB_TOPMOST "Sorry, the installer is already running!"
		Quit
	${EndIf}

	; --------
	
	# Running on Windows NT family?
	${IfNot} ${IsNT}
		MessageBox MB_TOPMOST|MB_ICONSTOP "Sorry, this application does *not* support Windows 9x or Windows ME!"
		ExecShell "open" "http://windows.microsoft.com/"
		Quit
	${EndIf}

	# Running on Windows Vista or later?
	${If} ${AtMostWinXP}
		MessageBox MB_TOPMOST|MB_ICONSTOP "Sorry, but your operating system is *not* supported anymore.$\nInstallation will be aborted!$\n$\nThe minimum supported platform is Windows Vista (Service Pack 2)."
		ExecShell "open" "http://windows.microsoft.com/"
		Quit
	${EndIf}

	# If on Windows Vista, is the required Service Pack installed?
	${If} ${IsWinVista}
	${AndIf} ${AtMostServicePack} 1
		MessageBox MB_TOPMOST|MB_ICONEXCLAMATION "This application requires Windows Vista with Service Pack 2 installed."
		${If} ${Cmd} `MessageBox MB_TOPMOST|MB_ICONQUESTION|MB_YESNO "Do you want to download Service Pack 2 for Windows Vista now?" IDYES`
			ExecShell "open" "https://www.microsoft.com/en-us/download/details.aspx?id=16468"
		${EndIf}
		Quit
	${EndIf}

	; --------

	${IfNot} ${UnattendedMode}
		!insertmacro MUI_LANGDLL_DISPLAY
	${EndIf}

	; --------

	UserInfo::GetAccountType
	Pop $0
	${If} $0 != "Admin"
		MessageBox MB_ICONSTOP|MB_TOPMOST "Your system requires administrative permissions in order to install this software."
		SetErrorLevel 740 ;ERROR_ELEVATION_REQUIRED
		Quit
	${EndIf}
FunctionEnd

Function un.onInit
	System::Call 'kernel32::CreateMutexA(i 0, i 0, t "{2B3D1EBF-B3B6-4E93-92B9-6853029A7162}") i .r1 ?e'
	Pop $0
	${If} $0 <> 0
		MessageBox MB_ICONSTOP|MB_TOPMOST "Sorry, the un-installer is already running!"
		Quit
	${EndIf}

	${IfNot} ${ForcedMode}
		!insertmacro MUI_LANGDLL_DISPLAY
	${EndIf}
	
	; --------

	UserInfo::GetAccountType
	Pop $0
	${If} $0 != "Admin"
		MessageBox MB_ICONSTOP|MB_TOPMOST "Your system requires administrative permissions in order to install this software."
		SetErrorLevel 740 ;ERROR_ELEVATION_REQUIRED
		Quit
	${EndIf}
FunctionEnd


;--------------------------------
;GUI initialization
;--------------------------------

Function MyGuiInit
	StrCpy $0 $HWNDPARENT
	System::Call "user32::SetWindowPos(i r0, i -1, i 0, i 0, i 0, i 0, i 3)"
	Aero::Apply
FunctionEnd

Function un.MyGuiInit
	StrCpy $0 $HWNDPARENT
	System::Call "user32::SetWindowPos(i r0, i -1, i 0, i 0, i 0, i 0, i 3)"
	Aero::Apply
FunctionEnd


;--------------------------------
;Macros & Auxiliary Functions
;--------------------------------

!macro PrintProgress Text
	SetDetailsPrint textonly
	DetailPrint '${Text}'
	SetDetailsPrint listonly
	Sleep 1000
!macroend

!macro CreateWebLink ShortcutFile TargetURL
	Push $0
	Push $1
	StrCpy $0 "${ShortcutFile}"
	StrCpy $1 "${TargetURL}"
	Call _CreateWebLink
	Pop $1
	Pop $0
!macroend

Function _CreateWebLink
	FlushINI "$0"
	SetFileAttributes "$0" FILE_ATTRIBUTE_NORMAL
	DeleteINISec "$0" "DEFAULT"
	DeleteINISec "$0" "InternetShortcut"
	WriteINIStr "$0" "DEFAULT" "BASEURL" "$1"
	WriteINIStr "$0" "InternetShortcut" "ORIGURL" "$1"
	WriteINIStr "$0" "InternetShortcut" "URL" "$1"
	WriteINIStr "$0" "InternetShortcut" "IconFile" "$SYSDIR\SHELL32.dll"
	WriteINIStr "$0" "InternetShortcut" "IconIndex" "150"
	FlushINI "$0"
	SetFileAttributes "$0" FILE_ATTRIBUTE_READONLY
FunctionEnd

!macro GetExecutableName OutVar
	${StdUtils.GetParameter} ${OutVar} "Update" ""
	${StdUtils.TrimStr} ${OutVar}
	${If} "${OutVar}" == ""
	${OrIfNot} ${ValidFileName} "${OutVar}"
		StrCpy ${OutVar} "x264_launcher.exe"
	${EndIf}
!macroend

!macro DisableNextButton TmpVar
	GetDlgItem ${TmpVar} $HWNDPARENT 1
	EnableWindow ${TmpVar} 0
!macroend

!macro DisableBackButton TmpVar
	GetDlgItem ${TmpVar} $HWNDPARENT 3
	EnableWindow ${TmpVar} 0
!macroend

!macro CleanUpFiles options
	Delete ${options} `$INSTDIR\*.exe`
	Delete ${options} `$INSTDIR\*.dll`
	Delete ${options} `$INSTDIR\*.txt`
	Delete ${options} `$INSTDIR\*.htm`
	Delete ${options} `$INSTDIR\*.html`
	
	RMDir /r ${options} `$INSTDIR\toolset`
	RMDir /r ${options} `$INSTDIR\imageformats`
	RMDir /r ${options} `$INSTDIR\sources`
!macroend

!macro SetControlEnabled item_id enable
	FindWindow $R0 "#32770" "" $HWNDPARENT
	${IfNot} $R0 == 0
		GetDlgItem $R1 $R0 ${item_id}
		EnableWindow $R1 ${enable}
	${EndIf}
!macroend

!macro SkipToNextPage
	GetDlgItem $R0 $HWNDPARENT 1
	System::Call "User32::PostMessage(i $HWNDPARENT, i ${WM_COMMAND}, i 1, i $R0)"
!macroend


;--------------------------------
;Install Files
;--------------------------------

Section "-PreInit"
	SetShellVarContext all
	SetOutPath "$INSTDIR"
SectionEnd

Section "-Clean Up Old Cruft"
	!insertmacro PrintProgress "$(X264_LANG_STATUS_CLEANUP)"
	!insertmacro CleanUpFiles ""
SectionEnd

Section "!Install Files"
	!insertmacro PrintProgress "$(X264_LANG_STATUS_INSTFILES)"
	
	DeleteOldBinary:
	!insertmacro GetExecutableName $R0
	ClearErrors
	Delete "$INSTDIR\$R0"
	
	${If} ${Errors}
		MessageBox MB_TOPMOST|MB_ICONSTOP|MB_RETRYCANCEL 'Could not delete old "$R0" file. Is program still running?' IDRETRY DeleteOldBinary
		Abort "Could not delete old binary!"
	${EndIf}

	File /a `/oname=$R0` `${X264_SOURCE_PATH}\x264_launcher.exe`

	File /a `${X264_SOURCE_PATH}\*.dll`
	File /a `${X264_SOURCE_PATH}\*.txt`
	File /a `${X264_SOURCE_PATH}\*.html`

	SetOutPath "$INSTDIR\imageformats"
	File /a /r `${X264_SOURCE_PATH}\imageformats\*.*`

	SetOutPath "$INSTDIR\toolset"
	File /a /r `${X264_SOURCE_PATH}\toolset\*.*`

	SetOutPath "$INSTDIR\sources"
	File /a /r `${X264_SOURCE_PATH}\sources\*.*`
SectionEnd

Section "-Write Uinstaller"
	!insertmacro PrintProgress "$(X264_LANG_STATUS_MAKEUNINST)"
	WriteUninstaller "$INSTDIR\Uninstall.exe"
SectionEnd

Section "-Create Shortcuts"
	!insertmacro MUI_STARTMENU_WRITE_BEGIN Application
		!insertmacro PrintProgress "$(X264_LANG_STATUS_SHORTCUTS)"
		CreateDirectory "$SMPROGRAMS\$StartMenuFolder"
		
		SetShellVarContext current
		
		Delete "$SMPROGRAMS\$StartMenuFolder\*.lnk"
		Delete "$SMPROGRAMS\$StartMenuFolder\*.pif"
		Delete "$SMPROGRAMS\$StartMenuFolder\*.url"
		
		SetShellVarContext all
		
		Delete "$SMPROGRAMS\$StartMenuFolder\*.lnk"
		Delete "$SMPROGRAMS\$StartMenuFolder\*.pif"
		Delete "$SMPROGRAMS\$StartMenuFolder\*.url"

		!insertmacro GetExecutableName $R0
		
		CreateShortCut "$SMPROGRAMS\$StartMenuFolder\Simple x264 Launcher.lnk"        "$INSTDIR\$R0" "" "$INSTDIR\$R0" 0
		CreateShortCut "$SMPROGRAMS\$StartMenuFolder\$(X264_LANG_LINK_LICENSE).lnk"   "$INSTDIR\LICENSE.html"
		CreateShortCut "$SMPROGRAMS\$StartMenuFolder\$(X264_LANG_LINK_MANUAL).lnk"    "$INSTDIR\README.html"
		CreateShortCut "$SMPROGRAMS\$StartMenuFolder\$(X264_LANG_LINK_CHANGELOG).lnk" "$INSTDIR\HISTORY.txt"
		CreateShortCut "$SMPROGRAMS\$StartMenuFolder\$(X264_LANG_LINK_UNINSTALL).lnk" "$INSTDIR\Uninstall.exe" "" "$INSTDIR\Uninstall.exe" 0
		
		!insertmacro CreateWebLink "$SMPROGRAMS\$StartMenuFolder\MuldeR's Homepage.url" "${MyWebSite}"
		!insertmacro CreateWebLink "$SMPROGRAMS\$StartMenuFolder\Doom9's Forum.url"     "http://forum.doom9.org/"
		!insertmacro CreateWebLink "$SMPROGRAMS\$StartMenuFolder\x264.com.url"          "http://x264.com/"
		!insertmacro CreateWebLink "$SMPROGRAMS\$StartMenuFolder\x265.com.url"          "http://x265.org/"
		!insertmacro CreateWebLink "$SMPROGRAMS\$StartMenuFolder\Avisynth Wiki.url"     "http://avisynth.nl/"
		!insertmacro CreateWebLink "$SMPROGRAMS\$StartMenuFolder\VapourSynth.com.url"   "http://www.vapoursynth.com/"

		${If} ${FileExists} "$SMPROGRAMS\$StartMenuFolder\Simple x264 Launcher.lnk"
			${StdUtils.InvokeShellVerb} $R1 "$SMPROGRAMS\$StartMenuFolder" "Simple x264 Launcher.lnk" ${StdUtils.Const.ShellVerb.PinToTaskbar}
			DetailPrint 'Pin: "$SMPROGRAMS\$StartMenuFolder\Simple x264 Launcher.lnk" -> $R1'
		${EndIf}
	!insertmacro MUI_STARTMENU_WRITE_END
SectionEnd

Section "-Update Registry"
	!insertmacro PrintProgress "$(X264_LANG_STATUS_REGISTRY)"

	!insertmacro GetExecutableName $R0
	WriteRegStr HKLM "${MyRegPath}" "InstallLocation" "$INSTDIR"
	WriteRegStr HKLM "${MyRegPath}" "ExecutableName" "$R0"
	WriteRegStr HKLM "${MyRegPath}" "DisplayIcon" "$INSTDIR\$R0,0"
	WriteRegStr HKLM "${MyRegPath}" "UninstallString" '"$INSTDIR\Uninstall.exe"'
	WriteRegStr HKLM "${MyRegPath}" "DisplayName" "Simple x264 Launcher v2"
	WriteRegStr HKLM "${MyRegPath}" "Publisher" "LoRd_MuldeR <mulder2@gmx.de>"
	WriteRegStr HKLM "${MyRegPath}" "DisplayVersion" "Build #${X264_BUILD} (${X264_DATE})"
	WriteRegStr HKLM "${MyRegPath}" "URLInfoAbout" "${MyWebSite}"
	WriteRegStr HKLM "${MyRegPath}" "URLUpdateInfo" "${MyWebSite}"
	
	DeleteRegKey HKCU "${AppPaths}\x264_launcher.exe"
	WriteRegStr HKLM "${AppPaths}\x264_launcher.exe" "" "$INSTDIR\$R0"
	WriteRegStr HKLM "${AppPaths}\x264_launcher.exe" "Path" "$INSTDIR"
SectionEnd

Section "-Finished"
	!insertmacro PrintProgress "$(MUI_TEXT_FINISH_TITLE)."
	${IfThen} ${UnattendedMode} ${|} SetAutoClose true ${|}
SectionEnd


;--------------------------------
;Uninstaller
;--------------------------------

Section "Uninstall"
	SetOutPath "$EXEDIR"
	!insertmacro PrintProgress "$(X264_LANG_STATUS_UNINSTALL)"

	; --------------
	; Startmenu
	; --------------
	
	!insertmacro MUI_STARTMENU_GETFOLDER Application $StartMenuFolder
	${IfNot} "$StartMenuFolder" == ""
		SetShellVarContext current
		${If} ${FileExists} "$SMPROGRAMS\$StartMenuFolder\Simple x264 Launcher.lnk"
			${StdUtils.InvokeShellVerb} $R1 "$SMPROGRAMS\$StartMenuFolder" "Simple x264 Launcher.lnk" ${StdUtils.Const.ShellVerb.UnpinFromTaskbar}
			DetailPrint 'Unpin: "$SMPROGRAMS\$StartMenuFolder\Simple x264 Launcher.lnk" -> $R1'
		${EndIf}
		${If} ${FileExists} "$SMPROGRAMS\$StartMenuFolder\*.*"
			Delete /REBOOTOK "$SMPROGRAMS\$StartMenuFolder\*.lnk"
			Delete /REBOOTOK "$SMPROGRAMS\$StartMenuFolder\*.url"
			RMDir "$SMPROGRAMS\$StartMenuFolder"
		${EndIf}
		
		SetShellVarContext all
		${If} ${FileExists} "$SMPROGRAMS\$StartMenuFolder\Simple x264 Launcher.lnk"
			${StdUtils.InvokeShellVerb} $R1 "$SMPROGRAMS\$StartMenuFolder" "Simple x264 Launcher.lnk" ${StdUtils.Const.ShellVerb.UnpinFromTaskbar}
			DetailPrint 'Unpin: "$SMPROGRAMS\$StartMenuFolder\Simple x264 Launcher.lnk" -> $R1'
		${EndIf}
		${If} ${FileExists} "$SMPROGRAMS\$StartMenuFolder\*.*"
			Delete /REBOOTOK "$SMPROGRAMS\$StartMenuFolder\*.lnk"
			Delete /REBOOTOK "$SMPROGRAMS\$StartMenuFolder\*.url"
			RMDir "$SMPROGRAMS\$StartMenuFolder"
		${EndIf}
	${EndIf}

	; --------------
	; Files
	; --------------

	ReadRegStr $R0 HKLM "${MyRegPath}" "ExecutableName"
	${IfThen} "$R0" == "" ${|} StrCpy $R0 "x264_launcher.exe" ${|}

	Delete /REBOOTOK "$INSTDIR\$R0"
	!insertmacro CleanUpFiles /REBOOTOK
	RMDir "$INSTDIR"

	; --------------
	; Registry
	; --------------
	
	DeleteRegKey HKLM "${MyRegPath}"
	DeleteRegKey HKCU "${MyRegPath}"

	DeleteRegKey HKLM "SOFTWARE\Wow6432Node\Microsoft\Windows\CurrentVersion\Uninstall\{54dcbccb-c905-46dc-b6e6-48563d0e9e55}"
	DeleteRegKey HKCU "SOFTWARE\Wow6432Node\Microsoft\Windows\CurrentVersion\Uninstall\{54dcbccb-c905-46dc-b6e6-48563d0e9e55}"
	
	DeleteRegKey HKLM "${AppPaths}\x264_launcher.exe"
	DeleteRegKey HKCU "${AppPaths}\x264_launcher.exe"

	MessageBox MB_YESNO|MB_TOPMOST "$(X264_LANG_UNINST_PERSONAL)" IDNO +3
	Delete "$LOCALAPPDATA\LoRd_MuldeR\Simple x264 Launcher\*.ini"
	RMDir "$LOCALAPPDATA\LoRd_MuldeR\Simple x264 Launcher"

	!insertmacro PrintProgress "$(MUI_UNTEXT_FINISH_TITLE)."
SectionEnd


;--------------------------------
;Check For Update Mode
;--------------------------------

Function SkipIfUnattended
	${IfThen} ${UnattendedMode} ${|} Abort ${|}
FunctionEnd

Function CheckForUpdate
	${If} "$INSTDIR" == ""
	${OrIf} "$INSTDIR" == "$EXEDIR"
	${OrIfNot} ${FileExists} "$INSTDIR\x264_launcher.exe"
		StrCpy $UpdateNotificationShown FALSE
		!insertmacro SetControlEnabled 1019 1
		!insertmacro SetControlEnabled 1001 1
		Return
	${Else}
		!insertmacro SetControlEnabled 1019 0
		!insertmacro SetControlEnabled 1001 0
		${If} "$UpdateNotificationShown" != TRUE
			StrCpy $UpdateNotificationShown TRUE
			MessageBox MB_ICONINFORMATION|MB_TOPMOST "$(X264_LANG_UNINST_UPDATE_MODE)"
			!insertmacro SkipToNextPage
		${EndIf}
	${EndIf}
FunctionEnd

Function un.CheckForcedUninstall
	${IfThen} ${ForcedMode} ${|} Abort ${|}
FunctionEnd


;--------------------------------
;Locked List
;--------------------------------

!macro _LockedListShow uinst
	!insertmacro MUI_HEADER_TEXT "$(X264_LANG_LOCKEDLIST_HEADER)" "$(X264_LANG_LOCKEDLIST_TEXT)"
	${If} ${UnattendedMode}
		!insertmacro DisableBackButton $R0
	${EndIf}
	${If} ${RunningX64}
		InitPluginsDir
		File /oname=$PLUGINSDIR\LockedList64.dll `${NSISDIR}\Plugins\LockedList64.dll`
	${EndIf}
	!insertmacro GetExecutableName $R0
	LockedList::AddModule "\$R0"
	${If} "$R0" != "x264_launcher.exe"
		LockedList::AddModule "\x264_launcher.exe"
	${EndIf}
	LockedList::AddModule "\Uninstall.exe"
	!if ${uinst} < 1
		LockedList::AddModule "\Au_.exe"
	!endif
	#LockedList::AddFolder "$INSTDIR"
	LockedList::Dialog /autonext /heading "$(X264_LANG_LOCKEDLIST_HEADING)" /noprograms "$(X264_LANG_LOCKEDLIST_NOPROG)" /searching  "$(X264_LANG_LOCKEDLIST_SEARCH)" /colheadings "$(X264_LANG_LOCKEDLIST_COLHDR1)" "$(X264_LANG_LOCKEDLIST_COLHDR2)"
	Pop $R0
!macroend

Function LockedListShow
	!insertmacro _LockedListShow 0
FunctionEnd

Function un.LockedListShow
	!insertmacro _LockedListShow 1
FunctionEnd


;--------------------------------
;Install Success
;--------------------------------

Function RunAppFunction
	!insertmacro DisableNextButton $R0
	!insertmacro GetExecutableName $R0
	${StdUtils.ExecShellAsUser} $R1 "$INSTDIR" "explore" ""
	${StdUtils.ExecShellAsUser} $R1 "$INSTDIR\$R0" "open" "--first-run"
FunctionEnd

Function ShowReadmeFunction
	!insertmacro DisableNextButton $R0
	${StdUtils.ExecShellAsUser} $R1 "$INSTDIR\README.html" "open" ""
FunctionEnd

Function .onInstSuccess
	${If} ${UnattendedMode}
		!insertmacro GetExecutableName $R0
		${StdUtils.ExecShellAsUser} $R1 "$INSTDIR" "explore" ""
		${StdUtils.ExecShellAsUser} $R1 "$INSTDIR\$R0" "open" "--first-run"
	${EndIf}
FunctionEnd
