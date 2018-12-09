ECHO OFF
ECHO usage:
ECHO   run "repair_header.bat" and then enter the file names
ECHO   when prompted or provide them in the command line. here
ECHO   are some example runs:
ECHO   C:\bin) repair_header.bat data\*.las
ECHO   C:\bin) repair_header.bat data\test001.las
ECHO   C:\bin) repair_header.bat data\path\*.las
ECHO   C:\bin) repair_header.bat data\flight\flight00*.las
IF "%1%" == "" GOTO GET_ALL
IF "%2%" == "" GOTO ACTION
:GET_ALL
SET /P F=input file (or wildcard):
FOR %%D in (%F%) DO START /wait lasinfo -quiet -peep -repair -version 1.1 -auto_date -i %%D
FOR %%D in (%F%) DO ECHO lasinfo -quiet -peep -repair -version 1.1 -auto_date -i %%D
GOTO END
:ACTION
ECHO OFF
FOR %%D in (%1) DO START /wait lasinfo -quiet -peep -repair -version 1.1 -auto_date -i %%D
FOR %%D in (%1) DO ECHO lasinfo -quiet -peep -repair -version 1.1 -auto_date -i %%D
:END
SET /P F=done. press ENTER
