Unicode true
Target amd64-unicode
!addplugindir /amd64-unicode "..\.tools\msys64\mingw64\share\nsis\Plugins\unicode"
!include "MUI2.nsh"
!include "LogicLib.nsh"
!include "x64.nsh"
!include "WinVer.nsh"
Name "Gozarno VPN"
!ifndef APP_VERSION
  !define APP_VERSION "1.4.1"
!endif
OutFile "..\dist\GozarnoVPN-${APP_VERSION}-Setup-x64.exe"
VIProductVersion "${APP_VERSION}.0"
VIAddVersionKey "ProductName" "Gozarno VPN"
VIAddVersionKey "ProductVersion" "${APP_VERSION}"
VIAddVersionKey "FileVersion" "${APP_VERSION}.0"
VIAddVersionKey "FileDescription" "Gozarno VPN Setup"
VIAddVersionKey "LegalCopyright" "Gozarno and contributors"
InstallDir "$PROGRAMFILES64\Gozarno VPN"
InstallDirRegKey HKLM "Software\Gozarno\Installer" "InstallDir"
RequestExecutionLevel admin
SetCompressor /SOLID lzma
!define MUI_ICON "..\src\openconnect-gui.ico"
!define MUI_UNICON "..\src\openconnect-gui.ico"
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "..\LICENSE.txt"
!insertmacro MUI_PAGE_COMPONENTS
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE "English"

Function .onInit
  ${IfNot} ${RunningX64}
    MessageBox MB_ICONSTOP "Gozarno VPN requires 64-bit Windows 10 or later."
    Abort
  ${EndIf}
  ${IfNot} ${AtLeastWin10}
    MessageBox MB_ICONSTOP "Gozarno VPN requires Windows 10 or later."
    Abort
  ${EndIf}
  SetRegView 64
  ReadRegStr $0 HKLM "Software\Gozarno\Installer" "InstallDir"
  ${If} $0 != ""
    StrCpy $INSTDIR $0
    ; Core is section 0, optional shared prerequisites are section 1.
    ; Upgrade the client by default without reinstalling shared drivers/runtimes.
    SectionSetFlags 1 0
  ${EndIf}
  ReadRegStr $0 HKLM "SYSTEM\CurrentControlSet\Control\Session Manager\Environment" "PROCESSOR_ARCHITECTURE"
  ${If} $0 == "ARM64"
    MessageBox MB_ICONSTOP "This release contains x64 network drivers and cannot be installed on ARM64."
    Abort
  ${EndIf}
FunctionEnd

; Check for locked application files BEFORE replacing any part of an installation.
; Never kill a VPN process: the user quits normally so routes and filters release.
Function CheckInstalledFile
  Exch $0
  Push $1
  IfFileExists "$0" 0 done
retry:
  System::Call 'kernel32::CreateFileW(w r0, i 0x40010000, i 7, p 0, i 3, i 0, p 0) p .r1'
  ${If} $1 == -1
    IfSilent blocked
    MessageBox MB_RETRYCANCEL|MB_ICONEXCLAMATION "Gozarno VPN or its routing helper is still running, or Windows has locked an installed file.$\r$\n$\r$\nDisconnect, choose Help & more > Quit (closing the window may only hide it), then click Retry. Your saved profiles will be preserved.$\r$\n$\r$\nFile: $0" IDRETRY retry
blocked:
    SetErrorLevel 2
    Abort
  ${EndIf}
  System::Call 'kernel32::CloseHandle(p r1)'
done:
  Pop $1
  Pop $0
FunctionEnd

Function EnsureApplicationClosed
  Push "$INSTDIR\GozarnoVPN.exe"
  Call CheckInstalledFile
  Push "$INSTDIR\backend\ProxiFyre.exe"
  Call CheckInstalledFile
FunctionEnd

Section "Gozarno VPN (required)" Core
  SectionIn RO
  Call EnsureApplicationClosed
  SetShellVarContext all
  SetOutPath "$INSTDIR"
  SetOverwrite on
  File /r "..\dist\GozarnoVPN\*.*"
  WriteUninstaller "$INSTDIR\Uninstall.exe"
  CreateDirectory "$SMPROGRAMS\Gozarno VPN"
  CreateShortcut "$SMPROGRAMS\Gozarno VPN\Gozarno VPN.lnk" "$INSTDIR\GozarnoVPN.exe"
  CreateShortcut "$SMPROGRAMS\Gozarno VPN\Uninstall.lnk" "$INSTDIR\Uninstall.exe"
  CreateShortcut "$DESKTOP\Gozarno VPN.lnk" "$INSTDIR\GozarnoVPN.exe"
  WriteRegStr HKLM "Software\Gozarno\Installer" "InstallDir" "$INSTDIR"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\GozarnoVPN" "DisplayName" "Gozarno VPN"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\GozarnoVPN" "DisplayVersion" "${APP_VERSION}"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\GozarnoVPN" "InstallLocation" "$INSTDIR"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\GozarnoVPN" "Publisher" "Gozarno"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\GozarnoVPN" "UninstallString" '$\"$INSTDIR\Uninstall.exe$\"'
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\GozarnoVPN" "DisplayIcon" "$INSTDIR\GozarnoVPN.exe"
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\GozarnoVPN" "NoModify" 1
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\GozarnoVPN" "NoRepair" 1
SectionEnd

; Isolated SOCKS and core VPN do not require the optional packet-filter driver.
Section /o "Optional app routing driver (not for SOCKS)" AppRouting
  !include "AppRoutingPrerequisites.nsh"
SectionEnd

Section "Uninstall"
  SetRegView 64
  SetShellVarContext all
  MessageBox MB_OKCANCEL "Disconnect and close Gozarno VPN before uninstalling. Saved profiles are preserved. Shared network drivers and other VPN clients are preserved." IDOK +2
  Abort
  Delete "$DESKTOP\Gozarno VPN.lnk"
  Delete "$SMPROGRAMS\Gozarno VPN\Gozarno VPN.lnk"
  Delete "$SMPROGRAMS\Gozarno VPN\Uninstall.lnk"
  RMDir "$SMPROGRAMS\Gozarno VPN"
  !include "uninstall-files.nsh"
  Delete "$INSTDIR\Uninstall.exe"
  RMDir "$INSTDIR"
  DeleteRegKey HKLM "Software\Gozarno\Installer"
  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\GozarnoVPN"
SectionEnd
