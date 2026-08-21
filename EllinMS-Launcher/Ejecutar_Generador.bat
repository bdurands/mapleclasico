@echo off
title Generador de downloads.xml
echo ==============================================
echo       Generando archivo downloads.xml
echo ==============================================
echo.

:: Intenta usar el script de PowerShell porque viene por defecto en Windows
powershell -ExecutionPolicy Bypass -File "%~dp0generate_manifest.ps1"

echo.
echo ==============================================
echo               Proceso terminado
echo ==============================================
pause
