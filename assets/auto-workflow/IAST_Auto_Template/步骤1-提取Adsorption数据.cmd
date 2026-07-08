@echo off
chcp 65001 >nul
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0run_iast_from_excel.ps1" -Mode Extract
if errorlevel 1 echo 步骤 1 失败，请查看上方错误信息。
pause
