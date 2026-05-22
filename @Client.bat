@echo off
chcp 65001 >nul
cd /d "%~dp0"

start "Stalker Online Client" "StalkerOnline.GameClient\build\Debug\StalkerOnline.GameClient.exe"