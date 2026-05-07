@echo off
setlocal
if not defined WATCOM set WATCOM=C:\WATCOM
set PATH=%WATCOM%\BINNT64;%WATCOM%\BINNT;%PATH%
wcl -bt=dos -ml -0 -ox -s -fm=rakuzitu Rakuzitu.c gameplay.c video.c timer.c keyboard.c player_assets.c opponent_assets.c gameover_asset.c
endlocal
