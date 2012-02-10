Function .onInstSuccess
	ExecShell "explore" '$INSTDIR'
	ExecShell "open" '$INSTDIR\ReadMe.txt'
FunctionEnd
