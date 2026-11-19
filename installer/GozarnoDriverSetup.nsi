; Offline prerequisites only: does not replace the running client or its profiles.
Unicode true
Target amd64-unicode
!include "LogicLib.nsh"
!include "x64.nsh"
!include "WinVer.nsh"
Name "Gozarno application routing setup"
!ifndef APP_VERSION
  !define APP_VERSION "1.4.2"
!endif
VIProductVersion "${APP_VERSION}.0"
VIAddVersionKey "ProductName" "Gozarno application routing setup"
VIAddVersionKey "FileVersion" "${APP_VERSION}.0"
VIAddVersionKey "FileDescription" "Offline application routing prerequisites"
OutFile "..\dist\GozarnoVPN\GozarnoDriverSetup.exe"
RequestExecutionLevel admin
SetCompressor /SOLID lzma
ShowInstDetails show
AutoCloseWindow true
Page instfiles

Function .onInit
  SetRegView 64
  ${IfNot} ${RunningX64}
    SetErrorLevel 1
    Abort
  ${EndIf}
  ${IfNot} ${AtLeastWin10}
    SetErrorLevel 1
    Abort
  ${EndIf}
  ReadRegStr $0 HKLM "SYSTEM\CurrentControlSet\Control\Session Manager\Environment" "PROCESSOR_ARCHITECTURE"
  ${If} $0 == "ARM64"
    SetErrorLevel 1
    Abort
  ${EndIf}
FunctionEnd

Section "Application routing prerequisites"
  !include "AppRoutingPrerequisites.nsh"
  IfRebootFlag reboot
  SetErrorLevel 0
  Goto done
reboot:
  SetErrorLevel 3010
done:
SectionEnd
