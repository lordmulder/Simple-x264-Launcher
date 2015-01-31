; ///////////////////////////////////////////////////////////////////////////////
; // Simple x264 Launcher
; // Copyright (C) 2004-2015 LoRd_MuldeR <MuldeR2@GMX.de>
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

!ifndef X264_BUILD
  !error "X264_BUILD is not defined !!!"
!endif
!ifndef X264_DATE
  !error "X264_DATE is not defined !!!"
!endif
!ifndef X264_OUTPUT_FILE
  !error "X264_OUTPUT_FILE is not defined !!!"
!endif
!ifndef X264_SOURCE_FILE
  !error "X264_SOURCE_FILE is not defined !!!"
!endif
!ifndef X264_UPX_PATH
  !error "X264_UPX_PATH is not defined !!!"
!endif

;Web-Site
!define MyWebSite "http://mulder.at.gg/"

;Installer file name
!define InstallerFileName "$PLUGINSDIR\x264_x64-SETUP-r${X264_BUILD}.exe"

;--------------------------------
;Includes
;--------------------------------

!include `LogicLib.nsh`
!include `StdUtils.nsh`


;--------------------------------
;Installer Attributes
;--------------------------------

XPStyle on
RequestExecutionLevel user
InstallColors /windows
Name "Simple x264 Launcher [Build #${X264_BUILD}]"
OutFile "${X264_OUTPUT_FILE}"
BrandingText "${X264_DATE} / Build #${X264_BUILD}"
Icon "${NSISDIR}\Contrib\Graphics\Icons\orange-install.ico"
ChangeUI all "${NSISDIR}\Contrib\UIs\sdbarker_tiny.exe"
ShowInstDetails show
AutoCloseWindow true
InstallDir ""


;--------------------------------
;Page Captions
;--------------------------------

SubCaption 0 " "
SubCaption 1 " "
SubCaption 2 " "
SubCaption 3 " "
SubCaption 4 " "


;--------------------------------
;Compressor
;--------------------------------

!packhdr "$%TEMP%\exehead.tmp" '"${X264_UPX_PATH}\upx.exe" --brute "$%TEMP%\exehead.tmp"'


;--------------------------------
;Reserved Files
;--------------------------------

ReserveFile "${NSISDIR}\Plugins\System.dll"
ReserveFile "${NSISDIR}\Plugins\StdUtils.dll"


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
VIAddVersionKey "LegalCopyright" "Copyright 2004-2015 LoRd_MuldeR"
VIAddVersionKey "LegalTrademarks" "GNU"
VIAddVersionKey "OriginalFilename" "x264_x64.${X264_DATE}.exe"
VIAddVersionKey "ProductName" "Simple x264 Launcher"
VIAddVersionKey "ProductVersion" "Build #${X264_BUILD} (${X264_DATE})"
VIAddVersionKey "Website" "${MyWebSite}"


;--------------------------------
;Installer initialization
;--------------------------------

Section "-LaunchTheInstaller"
	SetDetailsPrint textonly
	DetailPrint "Launching installer, please stay tuned..."
	SetDetailsPrint listonly
	
	InitPluginsDir
	SetOutPath "$PLUGINSDIR"
	
	SetOverwrite on
	File "/oname=${InstallerFileName}" "${X264_SOURCE_FILE}"

	; --------
	
	${If} "$EXEFILE" == "x264_launcher.exe"
		MessageBox MB_ICONSTOP|MB_TOPMOST "Sorry, you must NOT rename the installation program to 'x264_launcher.exe'. Please re-rename the installer executable file (e.g. to 'x264_x64-Setup.exe') and then try again!"
		Quit
	${EndIf}

	; --------

	${StdUtils.GetAllParameters} $R9 0
	${IfThen} "$R9" == "too_long" ${|} StrCpy $R9 "" ${|}

	${IfNot} "$R9" == ""
		DetailPrint "Parameters: $R9"
	${EndIf}

	; --------

	${Do}
		SetOverwrite ifdiff
		File "/oname=${InstallerFileName}" "${X264_SOURCE_FILE}"
		
		DetailPrint "ExecShellWait: ${InstallerFileName}"
		${StdUtils.ExecShellWaitEx} $R1 $R2 "${InstallerFileName}" "open" '$R9'
		DetailPrint "Result: $R1 ($R2)"
		
		${IfThen} $R1 == "no_wait" ${|} Goto RunSuccess ${|}
		
		${If} $R1 == "ok"
			Sleep 333
			HideWindow
			${StdUtils.WaitForProcEx} $R1 $R2
			Goto RunSuccess
		${EndIf}
		
		MessageBox MB_RETRYCANCEL|MB_ICONSTOP|MB_TOPMOST "Failed to launch the installer. Please try again!" IDCANCEL FallbackMode
	${Loop}
	

	; -----------

	FallbackMode:

	DetailPrint "Installer not launched yet, trying fallback mode!"

	SetOverwrite ifdiff
	File "/oname=${InstallerFileName}" "${X264_SOURCE_FILE}"

	ClearErrors
	ExecShell "open" "${InstallerFileName}" '$R9' SW_SHOWNORMAL
	IfErrors 0 RunSuccess

	ClearErrors
	ExecShell "" "${InstallerFileName}" '$R9' SW_SHOWNORMAL
	IfErrors 0 RunSuccess

	; --------

	SetDetailsPrint both
	DetailPrint "Failed to launch installer :-("
	SetDetailsPrint listonly

	SetErrorLevel 1
	Abort "Aborted."

	; --------
	
	RunSuccess:

	Delete /REBOOTOK "${InstallerFileName}"
	SetErrorLevel 0
SectionEnd
