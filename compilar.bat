@echo off
chcp 65001 >nul
setlocal EnableDelayedExpansion
title ArtiVox - Compilar ArtisVox One
cd /d "%~dp0"

echo ==================================================
echo    ArtisVox One 1.0.4 - Compilando
echo ==================================================
echo.

rem --- 1. Buscar Visual Studio (2026 o 2022) con C++ ---
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
    echo [ERROR] No encuentro Visual Studio.
    echo Instala Visual Studio con "Desarrollo para el escritorio con C++".
    pause & exit /b 1
)
set "VSPATH="
for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -prerelease -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSPATH=%%i"
if not defined VSPATH (
    echo [ERROR] Visual Studio no tiene instalado C++.
    echo Abre "Visual Studio Installer" y marca "Desarrollo para el escritorio con C++".
    pause & exit /b 1
)
echo Visual Studio: !VSPATH!

call "!VSPATH!\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 (
    echo [ERROR] No se pudo preparar el entorno de Visual Studio.
    pause & exit /b 1
)

rem --- 2. Comprobar CMake y Ninja (vienen con Visual Studio) ---
where cmake >nul 2>nul
if errorlevel 1 (
    echo [ERROR] Falta CMake. En Visual Studio Installer marca
    echo "Herramientas de CMake de C++ para Windows" y vuelve a ejecutar esto.
    pause & exit /b 1
)
where ninja >nul 2>nul
if errorlevel 1 (
    echo [ERROR] Falta Ninja. En Visual Studio Installer marca
    echo "Herramientas de CMake de C++ para Windows" y vuelve a ejecutar esto.
    pause & exit /b 1
)

rem --- 3. JUCE: si no hay Git, se descarga en zip (solo la primera vez) ---
if not exist "JUCE\CMakeLists.txt" (
    where git >nul 2>nul
    if errorlevel 1 (
        echo Descargando JUCE 8.0.10 ^(solo la primera vez^)...
        powershell -NoProfile -ExecutionPolicy Bypass -Command "$ProgressPreference='SilentlyContinue'; Invoke-WebRequest -Uri 'https://github.com/juce-framework/JUCE/archive/refs/tags/8.0.10.zip' -OutFile 'juce.zip'; Expand-Archive -Path 'juce.zip' -DestinationPath '_juce_tmp' -Force; Move-Item '_juce_tmp\JUCE-8.0.10' 'JUCE'; Remove-Item 'juce.zip'; Remove-Item '_juce_tmp' -Recurse -Force"
        if not exist "JUCE\CMakeLists.txt" (
            echo [ERROR] No se pudo descargar JUCE. Revisa tu conexion a internet.
            pause & exit /b 1
        )
    )
)

rem --- 4. Configurar (la primera vez descarga JUCE, necesita internet) ---
echo.
echo [1/3] Preparando el proyecto... (la primera vez tarda unos minutos)
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DARTISVOX_TESTS=ON
if errorlevel 1 (
    echo.
    echo [ERROR] Fallo al preparar el proyecto. Si cambiaste de version de Visual Studio,
    echo borra la carpeta "build" y vuelve a ejecutar compilar.bat.
    pause & exit /b 1
)

rem --- 5. Compilar ---
echo.
echo [2/3] Compilando ArtisVox One...
cmake --build build --config Release --target ArtisVoxOne_VST3 ArtisVoxOne_Standalone ArtisVoxOneTests
if errorlevel 1 (
    echo.
    echo [ERROR] Fallo al compilar. Haz una foto del error y pasamela.
    pause & exit /b 1
)

rem --- 6. Pruebas automaticas del motor de audio ---
echo.
echo [3/3] Probando el motor de audio en este ordenador...
"build\ArtisVoxOneTests_artefacts\Release\ArtisVoxOneTests.exe" --motor
if errorlevel 1 (
    echo.
    echo [AVISO] Alguna prueba ha fallado en este ordenador. Haz una foto y pasamela.
    echo NO uses el plugin con volumen alto hasta revisarlo.
    pause & exit /b 1
)

rem --- 7. Copiar el resultado ---
if exist "Compilado" rmdir /s /q "Compilado"
mkdir "Compilado"
xcopy /e /i /y /q "build\ArtisVoxOne_artefacts\Release\VST3\ArtisVox One.vst3" "Compilado\ArtisVox One.vst3" >nul
copy /y "build\ArtisVoxOne_artefacts\Release\Standalone\ArtisVox One.exe" "Compilado\" >nul

echo.
echo ==================================================
echo    LISTO
echo    Plugin:      Compilado\ArtisVox One.vst3
echo    Programa:    Compilado\ArtisVox One.exe  (para probar sin DAW)
echo.
echo    Siguiente paso: crear_instalador.bat
echo ==================================================
pause
