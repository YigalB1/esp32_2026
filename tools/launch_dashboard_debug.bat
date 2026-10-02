@echo off
REM Debug version: launches train_dashboard.py WITH a console window
REM visible, so you can see any error output if it crashes. Use
REM launch_dashboard.bat instead for normal day-to-day use (no
REM console at all) - this one is specifically for when something
REM goes wrong and you need to see why.
REM
REM Keep this .bat file in the SAME FOLDER as train_dashboard.py.

cd /d "%~dp0"
python train_dashboard.py

if errorlevel 1 pause
