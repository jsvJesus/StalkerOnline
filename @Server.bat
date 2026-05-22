@echo off
chcp 65001 >nul
cd /d "%~dp0"

start "Stalker Online Server" "StalkerOnline.Server\bin\Debug\net9.0\StalkerOnline.Server.exe"