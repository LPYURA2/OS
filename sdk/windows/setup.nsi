Unicode true
Target amd64-unicode
Name "StyleOS SDK 0.1 Preview"
!ifndef SDK_PAYLOAD
  !error "SDK_PAYLOAD must point to the staged SDK distribution"
!endif
!ifndef SDK_OUTPUT
  !error "SDK_OUTPUT must be the installer path"
!endif
OutFile "${SDK_OUTPUT}"
InstallDir "$LOCALAPPDATA\StyleOS-SDK\0.1-preview"
RequestExecutionLevel user
SetCompressor /SOLID lzma

Page directory
Page instfiles

Section
  SetShellVarContext current
  SetOutPath "$INSTDIR"
  File /r "${SDK_PAYLOAD}/*"
  CreateDirectory "$SMPROGRAMS\StyleOS SDK Preview"
  CreateShortCut "$SMPROGRAMS\StyleOS SDK Preview\Read me.lnk" "$INSTDIR\START-HERE-RU.txt"
  CreateShortCut "$SMPROGRAMS\StyleOS SDK Preview\Build presentations.lnk" "$INSTDIR\Build-Presentations.cmd"
  CreateShortCut "$SMPROGRAMS\StyleOS SDK Preview\New project.lnk" "$INSTDIR\New-Project.cmd"
  CreateShortCut "$SMPROGRAMS\StyleOS SDK Preview\Build project.lnk" "$INSTDIR\Build-Project.cmd"
SectionEnd
