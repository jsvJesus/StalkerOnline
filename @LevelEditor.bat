@echo off
chcp 65001 >nul
cd /d "%~dp0"

start "Stalker Online Level Editor" "StalkerOnline.GameClient\build\Debug\StalkerOnline.LevelEditor.exe"
