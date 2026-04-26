@echo off
setlocal
set "DIR=%~dp0"
set "FOUND=0"

for %%f in ("%DIR%*.tga") do (
    set "FOUND=1"
    echo Converting %%~nxf ...
    "%DIR%converter.exe" --slang "%%f" "%DIR%%%~nf.dds"
)

if "%FOUND%"=="0" echo No .tga files found in %DIR%
endlocal
