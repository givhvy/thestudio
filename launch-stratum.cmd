@echo off
setlocal
set "APP_DIR=%~dp0"
cd /d "%APP_DIR%"
start "Stratum DAW" /min cmd /c "npm start"
endlocal
