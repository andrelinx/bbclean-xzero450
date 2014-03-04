rem this batch creates NSIS installer for blackbox(es) in OUTDIR
@echo off
set PATH="C:\Program Files\WinRAR\";%path%

set OUTDIR=c:/_builds

echo copying NSIS installer files...

robocopy.exe installer %OUTDIR% blackbox.nsi installer.bmp 
robocopy.exe . %OUTDIR% GPL.txt
robocopy.exe 3rd_party\redistributables\2012u4 %OUTDIR%\redist vcredist_x86.exe vcredist_x64.exe
rem if %errorlevel% neq 0 goto TERM

echo NSIS creating installer.exe ...

set PATH=C:\Program Files (x86)\NSIS\unicode;%PATH%

cd %OUTDIR%
makensis.exe blackbox.nsi
rem makensis.exe /X"SetCompressor /FINAL lzma" myscript.nsi
if %errorlevel% neq 0 goto TERM

goto NOPAUSE

:TERM
pause

:NOPAUSE
