set CYGWIN=nodosfilewarning
set MAKE_FILE=C:\ZILOG\mingw32-make.exe

rem ***** A9 *****
%MAKE_FILE% VERSION=00020025 BOARD=A9
IF ERRORLEVEL 1 GOTO err
%MAKE_FILE% VERSION=00020026 BOARD=A9
IF ERRORLEVEL 1 GOTO err

rem ***** A10 *****
%MAKE_FILE% VERSION=00020026 BOARD=A10
IF ERRORLEVEL 1 GOTO err

rem ***** WIN32 *****
%MAKE_FILE% VERSION=00020026 BOARD=WIN32
IF ERRORLEVEL 1 GOTO err
%MAKE_FILE% VERSION=00020025 BOARD=WIN32
IF ERRORLEVEL 1 GOTO err

goto end

:err
@echo ************************************************************
@echo ! ERROR ! ERROR ! ERROR ! ERROR ! ERROR ! ERROR ! ERROR ! 
@echo ************************************************************
@pause

:end

