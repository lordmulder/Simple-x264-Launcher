#Create Shortcuts
MessageBox MB_ICONQUESTION|MB_TOPMOST|MB_YESNO "Create a desktop icon for Simple x264 Launcher?" /SD IDYES IDNO +2
CreateShortCut "$DESKTOP\Simple x264 Launcher.lnk" "$INSTDIR\x264_launcher.exe"
