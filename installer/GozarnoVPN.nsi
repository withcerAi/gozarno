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
  SetRegView 32
  ReadRegDWORD $0 HKLM "SOFTWARE\Microsoft\NET Framework Setup\NDP\v4\Full" "Release"
  SetRegView 64
  ${If} $0 < 461808
    MessageBox MB_ICONSTOP "Application routing needs .NET Framework 4.7.2 or later. Enable/update .NET through Windows Update, then run Setup again. Core VPN is installed and usable."
    Abort
  ${EndIf}
  SetOutPath "$PLUGINSDIR"
  ReadRegDWORD $0 HKLM "SOFTWARE\Microsoft\VisualStudio\14.0\VC\Runtimes\x64" "Major"
  ReadRegDWORD $1 HKLM "SOFTWARE\Microsoft\VisualStudio\14.0\VC\Runtimes\x64" "Minor"
  ${If} $0 < 14
    SetRegView 32
    ReadRegDWORD $0 HKLM "SOFTWARE\Microsoft\VisualStudio\14.0\VC\Runtimes\x64" "Major"
    ReadRegDWORD $1 HKLM "SOFTWARE\Microsoft\VisualStudio\14.0\VC\Runtimes\x64" "Minor"
    SetRegView 64
  ${EndIf}
  ; Some newer runtime installers omit the registry inventory. Check the actual
  ; installed x64 DLLs before trying to install an older bundled redistributable.
  ${If} $0 < 14
  ${OrIf} $1 < 44
    ClearErrors
    GetDLLVersion "$SYSDIR\vcruntime140.dll" $3 $4
    ${IfNot} ${Errors}
      IntOp $5 $3 >> 16
      IntOp $6 $3 & 0xFFFF
      ${If} $5 == 14
      ${AndIf} $6 >= 44
        ClearErrors
        GetDLLVersion "$SYSDIR\msvcp140.dll" $3 $4
        ${IfNot} ${Errors}
          IntOp $5 $3 >> 16
          IntOp $6 $3 & 0xFFFF
          ${If} $5 == 14
          ${AndIf} $6 >= 44
            StrCpy $0 14
            StrCpy $1 $6
          ${EndIf}
        ${EndIf}
      ${EndIf}
    ${EndIf}
  ${EndIf}
  ${If} $0 < 14
  ${OrIf} $1 < 44
    File /oname=vc-runtime.exe "..\build-dependencies\vc-runtime.exe"
    ExecWait '$\"$PLUGINSDIR\vc-runtime.exe$\" /install /quiet /norestart' $2
    ${If} $2 == 3010
      SetRebootFlag true
    ${ElseIf} $2 != 0
      MessageBox MB_ICONSTOP "Visual C++ runtime installation failed (code $2). Core VPN is usable; application routing prerequisites are incomplete."
      Abort
    ${EndIf}
  ${EndIf}
  ReadRegStr $0 HKLM "SYSTEM\CurrentControlSet\Services\ndisrd" "ImagePath"
  ${If} $0 == ""
    File /oname=packet-filter.msi "..\build-dependencies\packet-filter.msi"
    ExecWait '$\"$SYSDIR\msiexec.exe$\" /i $\"$PLUGINSDIR\packet-filter.msi$\" /qn /norestart /L*v $\"$TEMP\GozarnoVPN-driver-install.log$\"' $2
    ${If} $2 == 3010
      SetRebootFlag true
    ${ElseIf} $2 != 0
      MessageBox MB_ICONSTOP "Windows Packet Filter installation failed (code $2). See $TEMP\GozarnoVPN-driver-install.log. Application routing is unavailable."
      Abort
    ${EndIf}
  ${Else}
    ; Keep an existing shared driver; never overwrite another VPN's driver.
    GetDLLVersion "$SYSDIR\drivers\ndisrd.sys" $1 $2
    IntOp $3 $1 >> 16
    IntOp $4 $1 & 0xFFFF
    ${If} $3 != 3
    ${OrIf} $4 < 6
      MessageBox MB_ICONSTOP "An incompatible Windows Packet Filter is already installed. Gozarno will not replace a shared network driver. Core VPN remains usable; application routing needs a compatible 3.6.x driver."
      Abort
    ${EndIf}
  ${EndIf}
  ExecWait '$\"$SYSDIR\sc.exe$\" start ndisrd' $2
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
