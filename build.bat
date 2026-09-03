@echo off
setlocal

echo ========================================
echo quadrum build script
echo ========================================
echo.

:: set include flag if headers directory exists
set "INC_FLAGS="
if exist headers (
    set "INC_FLAGS=/Iheaders"
)

:: 1. generate ico if needed
if not exist quadrum.ico (
    cl /nologo /MD /DMAKE_ICO_STANDALONE %INC_FLAGS% icon.c user32.lib gdi32.lib /Fe:icon.exe
    if %ERRORLEVEL% NEQ 0 (
        echo [ERROR] Failed to compile icon generator.
        exit /b 1
    )
    icon.exe
    del icon.exe >nul 2>&1
)

:: 2. compile the embedded resources
set "RES_OBJ="
if exist quadrum.rc (
    rc /nologo /fo quadrum.res quadrum.rc
    if exist quadrum.res (
        set "RES_OBJ=quadrum.res"
    )
)

:: 3. compile quadrum with the icon resource linked in
cl /nologo /MD /O2 /fp:fast /W3 /std:c17 ^
    %INC_FLAGS% ^
    main.c %RES_OBJ% ^
    user32.lib gdi32.lib comctl32.lib winmm.lib comdlg32.lib ^
    /Fe:quadrum.exe ^
    /link /SUBSYSTEM:WINDOWS

:: preserves the compilation status before del
set "BUILD_RESULT=%ERRORLEVEL%"

:: optional: clean up intermediate build artifacts
del *.res *.obj 2>nul

if %BUILD_RESULT% EQU 0 (
    echo.
    echo [SUCCESS] Build completed: quadrum.exe
) else (
    echo.
    echo [ERROR] Compilation failed!
    exit /b 1
)