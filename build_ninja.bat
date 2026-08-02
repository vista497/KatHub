@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
set PATH=C:\Qt_new\Tools\Ninja;%PATH%
cd /d D:\vs\Project421\1_projectGPT\1_Main\KatHub\build\Desktop_Qt_6_7_3_MSVC2022_64bit-Debug
ninja kathub-backend
echo NINJA_EXIT=%ERRORLEVEL%
