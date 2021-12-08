@echo off

set app_name=render

if not defined DevEnvDir call vcvarsall x64
if not exist .\build mkdir .\build 
pushd .\build
echo -----------------
cl -Zi ..\code\win32_render.c -Fe%app_name%.exe user32.lib gdi32.lib
popd
