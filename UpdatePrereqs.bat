@ECHO OFF

:: BatchGotAdmin
:-------------------------------------
REM  --> Check for permissions
>nul 2>&1 "%SYSTEMROOT%\system32\cacls.exe" "%SYSTEMROOT%\system32\config\system"

REM --> If error flag set, we do not have admin.
if '%errorlevel%' NEQ '0' (
    echo Requesting administrative privileges...
    goto UACPrompt
) else ( goto gotAdmin )

:UACPrompt
    echo Set UAC = CreateObject^("Shell.Application"^) > "%temp%\getadmin.vbs"
    echo UAC.ShellExecute "%~s0", "", "", "runas", 1 >> "%temp%\getadmin.vbs"

    "%temp%\getadmin.vbs"
    exit /B

:gotAdmin
    if exist "%temp%\getadmin.vbs" ( del "%temp%\getadmin.vbs" )
    pushd "%CD%"
    CD /D "%~dp0"
:--------------------------------------  

python --version 2>NUL
IF ERRORLEVEL 1 (
    ECHO Install Python and ensure its on the path!!!!
	PAUSE
    EXIT /B
) ELSE (
    ECHO PYTHON FOUND
)

python -m ensurepip --default-pip
python -m pip install --upgrade pip
python -m pip install requests-aws
python -m pip install requests
python -m pip install py7zr

python ./PythonUtils/Validate3rdParty.py

PAUSE