@echo off
rem Points this clone at the versioned hooks in scripts/hooks.
git -C "%~dp0.." config core.hooksPath scripts/hooks || exit /b 1
echo core.hooksPath = scripts/hooks
