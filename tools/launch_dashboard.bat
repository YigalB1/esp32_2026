@echo off
REM Launches train_dashboard.py with NO console window - using pythonw
REM instead of python means there's no black terminal at all, so
REM there's nothing to accidentally close that would take the
REM dashboard down with it.
REM
REM Trade-off: if the dashboard crashes on startup, you won't see why
REM here - there's no window to show an error in. Use
REM launch_dashboard_debug.bat instead if you need to see what went
REM wrong.
REM
REM Keep this .bat file in the SAME FOLDER as train_dashboard.py - it
REM finds the script next to itself, so it still works correctly no
REM matter where that folder lives on disk.

cd /d "%~dp0"
start "" pythonw train_dashboard.py
