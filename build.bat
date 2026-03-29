@echo off
:: ─────────────────────────────────────────────────────────────────
::  Image Viewer v2.0 — Build Script
::  Requirements: gcc (MinGW) on PATH, stb_image.h in src/
:: ─────────────────────────────────────────────────────────────────

echo [1/3] Checking for stb_image.h ...
if not exist "src\stb_image.h" (
    echo.
    echo  ERROR: src\stb_image.h not found!
    echo  Download it from:
    echo  https://raw.githubusercontent.com/nothings/stb/master/stb_image.h
    echo  and place it in the src\ folder.
    echo.
    pause
    exit /b 1
)

if not exist "src\stb_image_resize2.h" (
    echo.
    echo  ERROR: src\stb_image_resize2.h not found!
    echo  Download it from:
    echo  https://raw.githubusercontent.com/nothings/stb/master/stb_image_resize2.h
    echo  Save it as: src\stb_image_resize2.h
    echo.
    pause
    exit /b 1
)
echo        stb_image_resize2.h found.

echo [2/3] Compiling ...
gcc ^
  src\stb_image_impl.c ^
  src\stb_image_resize2_impl.c ^
  src\image_io.c ^
  src\transform.c ^
  src\folder.c ^
  src\thumbs.c ^
  src\recent.c ^
  src\ui.c ^
  src\draw.c ^
  src\main.c ^
  -I include ^
  -I src ^
  -o image_viewer.exe ^
  -lgdi32 -lcomdlg32 -lshell32 -lshlwapi -lmsimg32 ^
  -mwindows -O2 -Wall 2>&1

if %ERRORLEVEL% == 0 (
    echo [3/3] Done!
    echo.
    echo  image_viewer.exe is ready.
    echo.
    echo  Usage:
    echo    image_viewer.exe [file]
    echo.
    echo  To set as default viewer:
    echo    Right-click any image ^> Open with ^> Browse ^> image_viewer.exe
    echo    Check "Always use this app"
) else (
    echo.
    echo  BUILD FAILED. See errors above.
)
pause