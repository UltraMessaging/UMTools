@echo off
REM Build umesnaprepo (Windows / MSVC).
REM
REM Set UMQ to the UMP SDK root before running, e.g.:
REM   set UMQ=C:\path\to\UMQ_6.17\Win2k-x86_64
REM   bld_umesnaprepo.bat

if "%UMQ%"=="" (
    echo ERROR: Set UMQ to the UMP SDK root, e.g. C:\path\to\UMQ_6.17\Win2k-x86_64
    exit /b 1
)

cl -DWIN32_LEAN_AND_MEAN -DWIN32_EXTRA_LEAN -DHAVE_CONFIG_H ^
    -I"%UMQ%\include" ^
    -I"%UMQ%\include\lbm" ^
    -I. ^
    /Ob1 /Oi /Ot /O2 /GL /Z7 -Fd.\ -c umesnaprepo.c getopt.c
if errorlevel 1 exit /b 1

link.exe /OUT:umesnaprepo.exe ws2_32.lib /NOLOGO /INCREMENTAL:no /LTCG /DEBUG ^
    /SUBSYSTEM:console umesnaprepo.obj getopt.obj ^
    "%UMQ%\lib\umestore.lib" "%UMQ%\lib\lbm.lib"
if errorlevel 1 exit /b 1

echo Built umesnaprepo.exe
