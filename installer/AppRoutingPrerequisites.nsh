  SetRegView 32
  ReadRegDWORD $0 HKLM "SOFTWARE\Microsoft\NET Framework Setup\NDP\v4\Full" "Release"
  SetRegView 64
  ${If} $0 < 461808
    MessageBox MB_ICONSTOP "Application routing needs .NET Framework 4.7.2 or later. Enable/update .NET through Windows Update, then run Setup again. Core VPN is installed and usable."
    SetErrorLevel 1
    Abort
  ${EndIf}
  InitPluginsDir
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
      SetErrorLevel 1
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
      SetErrorLevel 1
      Abort
    ${EndIf}
  ${Else}
    ; Keep an existing shared driver; never overwrite another VPN's driver.
    ClearErrors
    GetDLLVersion "$SYSDIR\drivers\ndisrd.sys" $1 $2
    ${If} ${Errors}
      MessageBox MB_ICONSTOP "The existing Windows Packet Filter driver could not be inspected. Repair it using its original installer, then try again."
      SetErrorLevel 1
      Abort
    ${EndIf}
    IntOp $3 $1 >> 16
    IntOp $4 $1 & 0xFFFF
    ${If} $3 != 3
    ${OrIf} $4 < 6
      MessageBox MB_ICONSTOP "An incompatible Windows Packet Filter is already installed. Gozarno will not replace a shared network driver. Core VPN remains usable; application routing needs a compatible 3.6.x driver."
      SetErrorLevel 1
      Abort
    ${EndIf}
  ${EndIf}
  ExecWait '$\"$SYSDIR\sc.exe$\" start ndisrd' $2
  ; 1056 means another client already started the shared driver.
  ${If} $2 != 0
  ${AndIf} $2 != 1056
    IfRebootFlag driver_done
    MessageBox MB_ICONSTOP "Windows Packet Filter could not start (code $2). Restart Windows and try setup again."
    SetErrorLevel 1
    Abort
  ${EndIf}
driver_done:
