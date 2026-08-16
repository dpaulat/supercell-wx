;-------------------------------------------------------------------------------
; Supercell Wx - thin NSIS MUI2 bootstrapper
;
; Extracts the VC++ redistributable and CPack MSI, installs the redist quietly,
; then runs msiexec. The MSI owns Add/Remove Programs; this .exe does not.
;
; Built after cpack by tools/build-windows-nsis-bootstrapper.ps1.
; Required defines:
;   SCWX_VERSION
;   SCWX_NSIS_PLUGINDIR  (data/nsis/plugins/x86-unicode - ShellExecAsUser.dll)
; Optional defines (defaults assume staged files in the makensis working directory):
;   SCWX_OUTFILE, SCWX_MSI (source path), SCWX_MSI_FILE (embedded name),
;   SCWX_VC_REDIST, SCWX_LICENSE, SCWX_HEADER_BMP, SCWX_WELCOME_BMP, SCWX_ICON
;-------------------------------------------------------------------------------

!ifndef SCWX_VERSION
  !error "SCWX_VERSION must be defined (e.g. makensis /DSCWX_VERSION=x.y.z)"
!endif

!ifndef SCWX_OUTFILE
  !define SCWX_OUTFILE "supercell-wx-v${SCWX_VERSION}-windows-x64.exe"
!endif
!ifndef SCWX_MSI_FILE
  !define SCWX_MSI_FILE "supercell-wx-v${SCWX_VERSION}-windows-x64.msi"
!endif
!ifndef SCWX_MSI
  !define SCWX_MSI "${SCWX_MSI_FILE}"
!endif
!ifndef SCWX_VC_REDIST
  !define SCWX_VC_REDIST "VC_redist.x64.exe"
!endif
!ifndef SCWX_LICENSE
  !define SCWX_LICENSE "License.rtf"
!endif
!ifndef SCWX_HEADER_BMP
  !define SCWX_HEADER_BMP "scwx-header.bmp"
!endif
!ifndef SCWX_WELCOME_BMP
  !define SCWX_WELCOME_BMP "scwx-welcome.bmp"
!endif
!ifndef SCWX_ICON
  !define SCWX_ICON "scwx-256.ico"
!endif

!include MUI2.nsh
!include LogicLib.nsh
!include x64.nsh

Unicode true
ManifestDPIAware true
RequestExecutionLevel admin
CRCCheck on
SetCompressor /SOLID lzma

; ShellExecAsUser (Unicode) - launch finish-page app as the non-elevated user.
; Plugin lives in the data submodule: data/nsis/plugins/x86-unicode/
!ifndef SCWX_NSIS_PLUGINDIR
  !error "SCWX_NSIS_PLUGINDIR must be defined (path to data/nsis/plugins/x86-unicode)"
!endif
!addplugindir /x86-unicode "${SCWX_NSIS_PLUGINDIR}"

Name "Supercell Wx"
OutFile "${SCWX_OUTFILE}"
InstallDir "$PROGRAMFILES64\Supercell Wx"
BrandingText "Supercell Wx"

; Silent installs: pass /S (and optional /D=dir as the last argument).

Icon "${SCWX_ICON}"

; Standard MUI layout: 150x57 header bitmap on the left, page title/subtitle on the right.
!define MUI_ICON "${SCWX_ICON}"
!define MUI_HEADERIMAGE
!define MUI_HEADERIMAGE_BITMAP "${SCWX_HEADER_BMP}"
!define MUI_HEADERIMAGE_BITMAP_NOSTRETCH
!define MUI_WELCOMEFINISHPAGE_BITMAP "${SCWX_WELCOME_BMP}"
!define MUI_ABORTWARNING

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "${SCWX_LICENSE}"
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES

; Launch unelevated: default MUI_FINISHPAGE_RUN inherits the installer's admin token.
!define MUI_FINISHPAGE_RUN
!define MUI_FINISHPAGE_RUN_FUNCTION Bootstrapper_LaunchApp
!define MUI_FINISHPAGE_RUN_TEXT "Launch Supercell Wx"
!define MUI_FINISHPAGE_LINK "View online documentation"
!define MUI_FINISHPAGE_LINK_LOCATION "https://supercell-wx.readthedocs.io/en/stable/"
!define MUI_FINISHPAGE_REBOOTLATER_DEFAULT
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_LANGUAGE "English"

Var RedistRebootRequired

Function Bootstrapper_LaunchApp
  ; ShellExecAsUser asks the shell to start the process unelevated (no explorer.exe path quirks).
  SetOutPath "$INSTDIR\bin"
  ShellExecAsUser::ShellExecAsUser "" "$INSTDIR\bin\supercell-wx.exe" "" ""
FunctionEnd

Function Bootstrapper_Fail
  ; Usage: Push "message" ; Call Bootstrapper_Fail
  Exch $R9
  ${If} ${Silent}
    SetErrorLevel 2
  ${Else}
    MessageBox MB_OK|MB_ICONSTOP "$R9"
  ${EndIf}
  Abort
FunctionEnd

Function Bootstrapper_StripTrailingSlash
  ; Ensures INSTALL_ROOT="..." is not broken by a trailing backslash escape.
  ; Drive roots must stay absolute: C:\ or C: → C:\. (NSIS often stores roots as C:).
  StrCpy $R8 $INSTDIR 1 -1
  ${If} $R8 == "\"
    StrLen $R9 $INSTDIR
    StrCpy $R8 $INSTDIR 1 1
    ${If} $R9 == 3
    ${AndIf} $R8 == ":"
      StrCpy $INSTDIR "$INSTDIR."
    ${Else}
      StrCpy $INSTDIR $INSTDIR -1
    ${EndIf}
  ${Else}
    StrLen $R9 $INSTDIR
    StrCpy $R8 $INSTDIR 1 1
    ${If} $R9 == 2
    ${AndIf} $R8 == ":"
      StrCpy $INSTDIR "$INSTDIR\."
    ${EndIf}
  ${EndIf}
FunctionEnd

Section "Install"
  StrCpy $RedistRebootRequired 0

  ${IfNot} ${RunningX64}
    Push "Supercell Wx requires 64-bit Windows."
    Call Bootstrapper_Fail
  ${EndIf}

  InitPluginsDir
  SetOutPath "$PLUGINSDIR"

  File "/oname=VC_redist.x64.exe" "${SCWX_VC_REDIST}"
  File "/oname=${SCWX_MSI_FILE}" "${SCWX_MSI}"

  DetailPrint "Installing Microsoft Visual C++ Redistributable (x64)..."
  ExecWait '"$PLUGINSDIR\VC_redist.x64.exe" /install /quiet /norestart' $0

  ; Accept 0 (success), 1638 (newer already installed), 3010 (reboot needed).
  ${If} $0 == 3010
    StrCpy $RedistRebootRequired 1
  ${ElseIf} $0 != 0
  ${AndIf} $0 != 1638
    Push "Visual C++ Redistributable installation failed (error $0)."
    Call Bootstrapper_Fail
  ${EndIf}

  Call Bootstrapper_StripTrailingSlash

  ; Always /qn /norestart: NSIS owns the UI; reboot prompt is deferred to Finish.
  DetailPrint "Installing Supercell Wx..."
  ExecWait '"$SYSDIR\msiexec.exe" /i "$PLUGINSDIR\${SCWX_MSI_FILE}" /qn /norestart INSTALL_ROOT="$INSTDIR"' $1

  ; msiexec: 0 success, 3010 success with reboot required.
  ${If} $1 == 3010
    SetRebootFlag true
  ${ElseIf} $1 != 0
    Push "Supercell Wx installation failed (msiexec error $1)."
    Call Bootstrapper_Fail
  ${ElseIf} $RedistRebootRequired == 1
    ; Prefer prompting after MSI so the product is installed before reboot.
    SetRebootFlag true
  ${EndIf}
SectionEnd
