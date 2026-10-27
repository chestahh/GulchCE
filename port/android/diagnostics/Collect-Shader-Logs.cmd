@echo off
cd /d "%~dp0"
where py >nul 2>nul
if errorlevel 1 (
    python "%~dp0collect_shader_logs.py" %*
) else (
    py -3 "%~dp0collect_shader_logs.py" %*
)
pause
