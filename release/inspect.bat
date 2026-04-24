@echo off
setlocal
set "DIR=%~dp0"
set "FOUND=0"

for %%f in ("%DIR%*.tga") do (
    set "FOUND=1"
    echo === %%~nf ===
    echo [source: %%~nxf]
    "%DIR%texdiag.exe" info "%%f"
    echo.
    if exist "%DIR%%%~nf.dds" (
        echo [converted: %%~nf.dds]
        "%DIR%texdiag.exe" info "%DIR%%%~nf.dds"
    ) else (
        echo [converted: %%~nf.dds not found - run convert.bat first]
    )
    echo.
)

if "%FOUND%"=="0" echo No .tga files found in %DIR%
endlocal
