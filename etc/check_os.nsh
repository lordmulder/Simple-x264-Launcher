!include "WinVer.nsh"

Function .onInit
	System::Call 'kernel32::CreateMutexA(i 0, i 0, t "{79d6a7e5-35af-47b1-a7d6-f20d0e5df772}") i .r1 ?e'
	Pop $0
	${If} $0 <> 0
		MessageBox MB_ICONSTOP|MB_TOPMOST "Sorry, the installer is already running!"
		Quit
	${EndIf}
	
	; --------
	
	${IfNot} ${IsNT}
		MessageBox MB_TOPMOST|MB_ICONSTOP "Sorry, this application does NOT support Windows 9x !!!"
		Quit
	${EndIf}
	
	${IfNot} ${AtLeastWinXP}
		MessageBox MB_TOPMOST|MB_ICONSTOP "Sorry, this application requires Windows XP or later!"
		Quit
	${EndIf}

	${If} ${IsWinXP}
	${AndIf} ${AtMostServicePack} 1
		MessageBox MB_TOPMOST|MB_ICONSTOP "Sorry, this application requires Service-Pack 2 for Windows XP or later!"
		ExecShell "open" "http://www.microsoft.com/download/en/details.aspx?id=24"
		Quit
	${EndIf}
	
	; --------
	
	UserInfo::GetAccountType
	Pop $0
	${If} $0 != "Admin"
		MessageBox MB_ICONSTOP|MB_TOPMOST "Your system requires administrative permissions in order to install this software."
		Quit
	${EndIf}
FunctionEnd
