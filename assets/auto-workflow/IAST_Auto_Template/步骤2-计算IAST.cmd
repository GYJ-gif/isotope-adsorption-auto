@echo off
chcp 65001 >nul
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0run_iast_from_excel.ps1" -Mode Calculate
if errorlevel 1 echo 步骤 2 失败，请查看上方错误信息。
pause
