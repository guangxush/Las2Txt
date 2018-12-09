ECHO OFF
ECHO usage:
ECHO   run "batch_change_version.bat" and then enter the two
ECHO   arguments when prompted or provide the two arguments
ECHO   in the command line. here are some example runs:
ECHO   C:\bin) batch_change_version.bat data\*.las 1.1
ECHO   C:\bin) batch_change_version.bat data\test001.las 1.2
ECHO   C:\bin) batch_change_version.bat data\path\*.las 1.0
ECHO   C:\bin) batch_change_version.bat data\flight\flight00*.las 1.0
IF "%1%" == "" GOTO GET_ALL
IF "%2%" == "" GOTO GET_VERSION
IF "%3%" == "" GOTO ACTION
:GET_ALL
SET /P F=input file (or wildcard):
SET /P V=version:
FOR %%D in (%F%) DO START /wait lasinfo -quiet -version %V% -i %%D
FOR %%D in (%F%) DO ECHO lasinfo -quiet -version %V% -i %%D
GOTO END
:GET_VERSION
SET /P V=version:
FOR %%D in (%1) DO START /wait lasinfo -quiet -version %V% -i %%D
FOR %%D in (%1) DO ECHO lasinfo -quiet -version %V% -i %%D
GOTO END
:ACTION
ECHO OFF
FOR %%D in (%1) DO START /wait lasinfo -quiet -version %2 -i %%D
FOR %%D in (%1) DO ECHO lasinfo -quiet -version %2 -i %%D
:END
SET /P F=done. press ENTER
