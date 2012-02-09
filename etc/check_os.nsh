Function .onInit
	System::Call 'kernel32::CreateMutexA(i 0, i 0, t "{79d6a7e5-35af-47b1-a7d6-f20d0e5df772}") i .r1 ?e'
	Pop $0
	${If} $0 <> 0
		MessageBox MB_ICONSTOP|MB_TOPMOST "Sorry, the installer is already running!"
		Quit
	${EndIf}

	; --------
	
	${IfNot} ${IsNT}
		MessageBox MB_TOPMOST|MB_ICONSTOP "Sorry, this application does NOT support Windows 9x or Windows ME!"
		Quit
	${EndIf}

	${If} ${AtMostWinNT4}
		${StdUtils.GetParameter} $R0 "Update" "?"
		${If} $R0 == "?"
			MessageBox MB_TOPMOST|MB_ICONSTOP "Sorry, your platform is not supported anymore. Installation aborted!$\nThe minimum required platform is Windows 2000."
		${Else}
			MessageBox MB_TOPMOST|MB_ICONSTOP "Sorry, your platform is not supported anymore. Update not possible!$\nThe minimum required platform is Windows 2000."
		${EndIf}
		Quit
	${EndIf}
FunctionEnd
