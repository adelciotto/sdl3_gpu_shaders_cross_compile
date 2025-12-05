@echo off
setlocal enabledelayedexpansion
cd /D "%~dp0"
set source_dir=%CD:\=/%

:: --- Unpack Arguments -------------------------------------------------------
for %%a in (%*) do set "%%~a=1"
if not "%release%"=="1" set debug=1
if "%debug%"=="1" set release=0 && echo [debug mode]
if "%release%"=="1" set debug=0 && echo [release mode]

:: --- Unpack Command line Build Arguments ------------------------------------
:: None for now...

:: --- Compile/Link Definitions -----------------------------------------------
set cl_common=/nologo /MD /EHsc /std:c++17 ^
              /I..\src /I..\extern\HandmadeMath /I..\extern\SDL3\win\include /I..\extern\imgui
set cl_debug=call cl /Zi /Od /DBUILD_DEBUG /DRESOURCES_PATH=\"%source_dir%/\" /I..\extern\SDL3_shadercross\win\include %cl_common%
set cl_release=call cl /O2 %cl_common%
set cl_link_common=..\extern\SDL3\win\lib\x64\SDL3.lib shell32.lib /subsystem:console
set cl_link_debug=/link ..\extern\SDL3_shadercross\win\lib\SDL3_shadercross.lib %cl_link_common%
set cl_link_release=/link %cl_link_common%
if "%debug%"=="1" set cl_compile=%cl_debug%
if "%release%"=="1" set cl_compile=%cl_release%
if "%debug%"=="1" set cl_link=%cl_link_debug%
if "%release%"=="1" set cl_link=%cl_link_release%

:: --- Shader Compile Definitions ---------------------------------------------
set shadercross=call ..\extern\SDL3_shadercross\win\bin\shadercross.exe
set shadercross_vertex=%shadercross% -t vertex -DVERTEX_SHADER
set shadercross_fragment=%shadercross% -t fragment -DFRAGMENT_SHADER

:: --- Prep Directories -------------------------------------------------------
set build_dir_debug=build_debug
set build_dir_release=build_release
if "%debug%"=="1" set build_dir=%build_dir_debug%
if "%release%"=="1" set build_dir=%build_dir_release%
if not exist %build_dir% mkdir %build_dir%
if not exist %build_dir%\res mkdir %build_dir%\res

:: --- Build Everything -------------------------------------------------------
pushd %build_dir%

if "%release%"=="1" (
echo Compiling shaders...
%shadercross_vertex% ..\src\fullscreen.hlsl -o res\fullscreen.dxil || exit /b 1
%shadercross_fragment% ..\src\fbm_warp.hlsl -o res\fbm_warp.dxil || exit /b 1
)

echo Compiling source files...
%cl_compile% ..\src\sdl3_gpu_shaders_cross_compile.cpp ^
             ..\extern\imgui\imgui.cpp ^
             ..\extern\imgui\imgui_demo.cpp ^
             ..\extern\imgui\imgui_draw.cpp ^
             ..\extern\imgui\imgui_impl_sdl3.cpp ^
             ..\extern\imgui\imgui_impl_sdlgpu3.cpp ^
             ..\extern\imgui\imgui_tables.cpp ^
             ..\extern\imgui\imgui_widgets.cpp ^
             %cl_link% /out:sdl3_gpu_shaders_cross_compile.exe || exit /b 1

popd

:: --- Copy DLL's -------------------------------------------------------------
if not exist %build_dir%\SDL3.dll copy extern\SDL3\win\lib\x64\SDL3.dll %build_dir% >nul
if "%debug%"=="1" (
if not exist %build_dir%\SDL3_shadercross.dll copy extern\SDL3_shadercross\win\bin\SDL3_shadercross.dll %build_dir% >nul
if not exist %build_dir%\dxcompiler.dll copy extern\SDL3_shadercross\win\bin\dxcompiler.dll %build_dir% >nul
if not exist %build_dir%\dxil.dll copy extern\SDL3_shadercross\win\bin\dxil.dll %build_dir% >nul
if not exist %build_dir%\spirv-cross-c-shared.dll copy extern\SDL3_shadercross\win\bin\spirv-cross-c-shared.dll %build_dir% >nul
)

echo Done^^!
