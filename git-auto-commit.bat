@echo off
REM Git Auto-Commit Helper
REM Usage: git-auto-commit.bat [commit message]
REM Scans for changes and commits everything tracked

cd /d "%~dp0"

REM Check if there's anything to commit
git status --porcelain >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo [!] Not a git repository or git not available
    exit /b 1
)

git status --porcelain | findstr "." >nul
if %ERRORLEVEL% NEQ 0 (
    echo [.] Nothing changed, nothing to commit.
    exit /b 0
)

REM Show what's changed
echo [i] Changes detected:
git status --short
echo.

REM Commit with provided message or auto-generate one
if "%~1"=="" (
    for /f "usebackq delims=" %%a in (`powershell -Command "Get-Date -Format 'yyyy-MM-dd HH:mm'"`) do set TIMESTAMP=%%a
    git add -A
    git commit -m "🔄 Auto-commit %TIMESTAMP%"
) else (
    git add -A
    git commit -m "%~1"
)

if %ERRORLEVEL% EQU 0 (
    echo [✓] Committed successfully!
) else (
    echo [!] Commit failed.
)
