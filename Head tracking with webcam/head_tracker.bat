@echo off
setlocal
cd /d "%~dp0"
title Head Tracker
where py >nul 2>nul
if %ERRORLEVEL%==0 (
    py head_tracker.py
) else (
    python head_tracker.py
)
if errorlevel 1 (
    echo.
    echo Head tracker exited with an error. Press any key to close.
    pause >nul
)
