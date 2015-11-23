

@echo off

set CURRDIR=%~dp0
set TARGETFILE=%~1
set MAXVERSION=%~2

REM This file is now mostly obsolete, and is only used to generate
REM a plugin ini in VS post-build step so we have a correct ini
REM file when starting debugging 

echo Generating new plugin ini file @ %TARGETFILE%

SET MAX_ENV_VAR=ADSK_3DSMAX_x64_%MAXVERSION%
CALL SET MAX_PATH=%%%MAX_ENV_VAR%%%

IF EXIST "%TARGETFILE%" del "%TARGETFILE%"
REM add in original plugins
@echo [Directories]                                                     >> "%TARGETFILE%"
@echo Additional MAX plug-ins=%MAX_PATH%plugins                         >> "%TARGETFILE%"
@echo Fabric Plugins=%~dp1                                              >> "%TARGETFILE%"
@echo [Help]                                                            >> "%TARGETFILE%"
@echo mental ray Help=http://www.autodesk.com/mentalray-help-2013-enu   >> "%TARGETFILE%"

rem echo on
