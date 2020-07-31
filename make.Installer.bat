@echo off

set RELEASE_PATH=.\x64\Release
set INSTALLER_PATH=.\Undercover

mkdir %INSTALLER_PATH% > NULL 2>&1

xcopy "%RELEASE_PATH%\UC.Operator.exe" "%INSTALLER_PATH%" /D /Y
xcopy "%RELEASE_PATH%\UC.Operator.txt" "%INSTALLER_PATH%" /D /Y
xcopy "%RELEASE_PATH%\UC.Agent.dll" "%INSTALLER_PATH%" /D /Y
xcopy "%RELEASE_PATH%\UC.Tinker.dll" "%INSTALLER_PATH%" /D /Y
xcopy "%RELEASE_PATH%\UC.Logger.dll" "%INSTALLER_PATH%" /D /Y
xcopy "%RELEASE_PATH%\UC.Utils.dll" "%INSTALLER_PATH%" /D /Y
xcopy "%RELEASE_PATH%\msdia140.dll" "%INSTALLER_PATH%" /D /Y
xcopy "%RELEASE_PATH%\UC.Logger.ini" "%INSTALLER_PATH%" /D /Y
xcopy ".\console.bat" "%INSTALLER_PATH%" /D /Y

pause