@echo off
setlocal

REM Set the paths to your shader source files
set VERT=triangle.vert
set FRAG=triangle.frag

REM Set the output file names
set VERT_SPV=triangle.vert.spv
set FRAG_SPV=triangle.frag.spv
set VERT_INC=triangle.vert.inc
set FRAG_INC=triangle.frag.inc

REM Path to glslangValidator
where glslangValidator >nul 2>nul
if errorlevel 1 (echo glslangValidator not found. Please ensure it is installed and in your PATH.
	exit /b 1
)

REM Path to xxd
where xxd >nul 2>nul
if errorlevel 1 (echo xxd not found. Please ensure it is installed and in your PATH.
	exit /b 1
)

REM Compile GLSL to SPIR-V
echo Compiling %VERT% to %VERT_SPV%...
glslangValidator -V %VERT% -o %VERT_SPV%
if errorlevel 1 (
	echo Failed to compile %VERT%.
	exit /b 1
)

echo Compiling %FRAG% to %FRAG_SPV%...
glslangValidator -V %FRAG% -o %FRAG_SPV%
if errorlevel 1 (
	echo Failed to compile %FRAG%.
	exit /b 1
)

REM Convert SPIR-V to C header files
echo Converting %VERT_SPV% to %VERT_INC%...
xxd -i %VERT_SPV% > %VERT_INC%
if errorlevel 1 (
	echo Failed to convert %VERT_SPV% to %VERT_INC%.
	exit /b 1
)
echo Converting %FRAG_SPV% to %FRAG_INC%...
xxd -i %FRAG_SPV% > %FRAG_INC%
if errorlevel 1 (
	echo Failed to convert %FRAG_SPV% to %FRAG_INC%.
	exit /b 1
)

echo Shader compilation and conversion completed successfully.
endlocal