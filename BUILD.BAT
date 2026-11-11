@ECHO OFF
copy /y d:\proyectos\VGA256\Source\vga256.c .\
copy /y d:\proyectos\VGA256\Source\vga256.h .\
rem *** OPENWATCOM C++ 2.0 *** 
SET WATCOM=D:\FICHEROS\OC
SET OLDPATH=%PATH%
SET PATH=%WATCOM%\BINNT;%WATCOM%\BINW;%WATCOM%\BINP;%PATH%
SET EDPATH=%WATCOM%\EDDAT
SET INCLUDE=%WATCOM%\H;%WATCOM%\MFC\INCLUDE;%WATCOM%\H\NT;INCLUDE;
SET LIB=LIB;
WCL386 -oneatx -ohirbk -ol -ol+ -oi -ei -zp16 -6r -fpi87 -fp6 -mf -s -ri -zm /bt=dos /l=zrdx /fhwe /"OPTION ELIMINATE" /"OPTION VFREMOVAL" ROLEX386.C VGA256.C
SET WATCOM=
SET PATH=%OLDPATH%
SET EDPATH=
SET INCLUDE=
SET LIB=

:FIN
DEL *.obj
DEL *.pch
DEL *.tds
DEL *.bak
DEL *.err
DEL *.map
DEL *.sym

D:\FICHEROS\UTIL\UPX --ultra-brute --no-lzma --overlay=strip ROLEX386.EXE