@echo off
chcp 65001 >nul
setlocal EnableDelayedExpansion
title ArtiVox - Crear instalador de ArtisVox One
cd /d "%~dp0"

rem --- Buscar Inno Setup (versiones 7 y 6) ---
set "ISCC="
for %%V in (7 6) do (
    if not defined ISCC if exist "%ProgramFiles(x86)%\Inno Setup %%V\ISCC.exe" set "ISCC=%ProgramFiles(x86)%\Inno Setup %%V\ISCC.exe"
    if not defined ISCC if exist "%ProgramFiles%\Inno Setup %%V\ISCC.exe" set "ISCC=%ProgramFiles%\Inno Setup %%V\ISCC.exe"
    if not defined ISCC if exist "%LocalAppData%\Programs\Inno Setup %%V\ISCC.exe" set "ISCC=%LocalAppData%\Programs\Inno Setup %%V\ISCC.exe"
)

rem --- Si no esta en las carpetas normales, mirar en el registro de Windows ---
if not defined ISCC (
    for %%K in ("Inno Setup 7_is1" "Inno Setup 6_is1") do (
        for %%R in (HKLM HKCU) do (
            for %%W in ("" "\WOW6432Node") do (
                if not defined ISCC (
                    for /f "tokens=2,*" %%A in ('reg query "%%R\SOFTWARE%%~W\Microsoft\Windows\CurrentVersion\Uninstall\%%~K" /v InstallLocation 2^>nul ^| find "InstallLocation"') do (
                        if exist "%%B\ISCC.exe" set "ISCC=%%B\ISCC.exe"
                        if not defined ISCC if exist "%%BISCC.exe" set "ISCC=%%BISCC.exe"
                    )
                )
            )
        )
    )
)

rem --- Ultimo intento: si ISCC esta en el PATH ---
if not defined ISCC (
    for /f "delims=" %%P in ('where ISCC.exe 2^>nul') do if not defined ISCC set "ISCC=%%P"
)

if not defined ISCC (
    echo [ERROR] No encuentro Inno Setup.
    echo Comprueba que esta instalado. Si lo instalaste en otra carpeta,
    echo copia la ruta de ISCC.exe y pegala aqui abajo, luego pulsa Enter.
    set /p "ISCC=Ruta de ISCC.exe: "
    set "ISCC=!ISCC:"=!"
)
if not exist "!ISCC!" (
    echo [ERROR] No existe: !ISCC!
    pause & exit /b 1
)
echo Inno Setup: !ISCC!

if not exist "Compilado\ArtisVox One.vst3" (
    echo [ERROR] Primero ejecuta compilar.bat para crear el plugin.
    pause & exit /b 1
)

echo Creando el instalador...
"!ISCC!" "Instalador_fuente\ArtisVoxOne.iss"
if errorlevel 1 (
    echo [ERROR] No se pudo crear el instalador. Haz una foto del error y pasamela.
    pause & exit /b 1
)

echo.
echo ==================================================
echo    LISTO: carpeta Instalador\ArtisVoxOne_Setup_v1.0.5_Windows.exe
echo    Ese es el archivo que compartes o subes a Payhip.
echo ==================================================
pause
