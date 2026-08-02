@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
cd /d D:\vs\Project421\1_projectGPT\1_Main\KatHub
"C:\Qt_new\Tools\Ninja\ninja.exe" -C build\Desktop_Qt_6_7_3_MSVC2022_64bit-Debug\backend kathub-backend
echo NINJA_EXIT=%ERRORLEVEL%
