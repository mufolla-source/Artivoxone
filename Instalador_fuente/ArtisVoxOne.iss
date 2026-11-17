; ============================================================
;  Instalador oficial ArtiVox - ArtisVox One
;  Se compila con Inno Setup 6 o 7 (gratis): crear_instalador.bat
; ============================================================
#define AppName "ArtisVox One"
#define AppVersion "1.0.3"
#define Publisher "ArtiVox"
#define AppURL "https://artivoxaudio.com"

[Setup]
AppId={{8C3E5A27-41D9-4B6F-A2E8-5F7C1D9B3E46}
AppName={#AppName}
AppVersion={#AppVersion}
AppVerName={#AppName} {#AppVersion}
AppPublisher={#Publisher}
AppPublisherURL={#AppURL}
AppSupportURL={#AppURL}
AppUpdatesURL={#AppURL}
AppCopyright=© 2026 ArtiVox
VersionInfoVersion={#AppVersion}
VersionInfoCompany=ArtiVox
VersionInfoDescription=ArtisVox One - Cadena vocal
DefaultDirName={autopf}\ArtiVox\ArtisVox One
DefaultGroupName=ArtiVox
DisableProgramGroupPage=yes
DisableDirPage=yes
OutputDir=..\Instalador
OutputBaseFilename=ArtisVoxOne_Setup_v{#AppVersion}_Windows
SetupIconFile=artivox.ico
UninstallDisplayIcon={app}\ArtisVox One.exe
UninstallDisplayName=ArtisVox One by ArtiVox
WizardStyle=modern
WizardImageFile=wizard.bmp,wizard_2x.bmp
WizardSmallImageFile=small.bmp,small_2x.bmp
Compression=lzma2/ultra64
SolidCompression=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
CloseApplications=yes
ShowLanguageDialog=yes

[Languages]
Name: "es"; MessagesFile: "compiler:Languages\Spanish.isl"
Name: "en"; MessagesFile: "compiler:Default.isl"
Name: "pt"; MessagesFile: "compiler:Languages\BrazilianPortuguese.isl"

[Messages]
es.WelcomeLabel2=Se instalará [name/ver] en tu ordenador.%n%nLa cadena vocal profesional todo en uno: afinación, EQ, compresor, de-esser, reverb, delay y Vocal Assist.%n%nCierra FL Studio (o tu DAW) antes de continuar.
en.WelcomeLabel2=This will install [name/ver] on your computer.%n%nThe all-in-one professional vocal chain: tuning, EQ, compressor, de-esser, reverb, delay and Vocal Assist.%n%nPlease close FL Studio (or your DAW) before continuing.
pt.WelcomeLabel2=Isto vai instalar [name/ver] no seu computador.%n%nA cadeia vocal profissional tudo em um: afinação, EQ, compressor, de-esser, reverb, delay e Vocal Assist.%n%nFeche o FL Studio (ou sua DAW) antes de continuar.
es.FinishedLabel=ArtisVox One se ha instalado correctamente.%n%nEn FL Studio: Options > Manage plugins > Find installed plugins.%n%nGracias por usar ArtiVox.
en.FinishedLabel=ArtisVox One has been installed successfully.%n%nIn FL Studio: Options > Manage plugins > Find installed plugins.%n%nThanks for using ArtiVox.
pt.FinishedLabel=ArtisVox One foi instalado com sucesso.%n%nNo FL Studio: Options > Manage plugins > Find installed plugins.%n%nObrigado por usar ArtiVox.

[CustomMessages]
es.CompVST3=Plugin VST3 (FL Studio, Ableton, Cubase, Reaper...)
en.CompVST3=VST3 plugin (FL Studio, Ableton, Cubase, Reaper...)
pt.CompVST3=Plugin VST3 (FL Studio, Ableton, Cubase, Reaper...)
es.CompApp=Aplicación independiente (sin DAW)
en.CompApp=Standalone application (no DAW)
pt.CompApp=Aplicativo independente (sem DAW)
es.VisitWeb=Visitar artivoxaudio.com
en.VisitWeb=Visit artivoxaudio.com
pt.VisitWeb=Visitar artivoxaudio.com
es.TypeFull=Instalación completa
en.TypeFull=Full installation
pt.TypeFull=Instalação completa
es.TypeCustom=Personalizada
en.TypeCustom=Custom
pt.TypeCustom=Personalizada

[Types]
Name: "full"; Description: "{cm:TypeFull}"
Name: "custom"; Description: "{cm:TypeCustom}"; Flags: iscustom

[Components]
Name: "vst3"; Description: "{cm:CompVST3}"; Types: full custom; Flags: fixed
Name: "app";  Description: "{cm:CompApp}";  Types: full

[Files]
Source: "..\Compilado\ArtisVox One.vst3\*"; DestDir: "{commoncf64}\VST3\ArtisVox One.vst3"; Components: vst3; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "..\Compilado\ArtisVox One.exe"; DestDir: "{app}"; Components: app; Flags: ignoreversion
Source: "..\LEEME.txt"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\ArtisVox One"; Filename: "{app}\ArtisVox One.exe"; Components: app
Name: "{group}\artivoxaudio.com"; Filename: "{#AppURL}"
Name: "{group}\{cm:UninstallProgram,ArtisVox One}"; Filename: "{uninstallexe}"

[Run]
Filename: "{#AppURL}"; Description: "{cm:VisitWeb}"; Flags: postinstall shellexec nowait skipifsilent unchecked

[UninstallDelete]
Type: filesandordirs; Name: "{commoncf64}\VST3\ArtisVox One.vst3"
