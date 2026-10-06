#!/bin/bash
# qiin-tabtip.exe build script

set -e

echo "========================================"
echo "qiin-tabtip build"
echo "========================================"
echo ""

# Check the source file.
if [ ! -f "qiin-tabtip.cpp" ]; then
    echo "Error: qiin-tabtip.cpp was not found"
    exit 1
fi

# Detect the compiler.
COMPILER=""
COMPILE_CMD=""

if command -v cl.exe &> /dev/null; then
    COMPILER="MSVC"
    COMPILE_CMD="cl.exe /EHsc /std:c++17 /DUNICODE /D_UNICODE /Os /Fe:qiin-tabtip.exe qiin-tabtip.cpp shell32.lib user32.lib advapi32.lib ole32.lib oleaut32.lib uuid.lib /link /SUBSYSTEM:CONSOLE /nologo"
elif command -v g++ &> /dev/null; then
    COMPILER="MinGW/GCC"
    COMPILE_CMD="g++ -std=c++17 -DUNICODE -D_UNICODE -Os -s -o qiin-tabtip.exe qiin-tabtip.cpp -lshell32 -luser32 -ladvapi32 -lole32 -loleaut32 -luuid -static-libgcc -static-libstdc++ -Wl,--gc-sections -municode"
elif command -v clang++ &> /dev/null; then
    COMPILER="Clang"
    COMPILE_CMD="clang++ -std=c++17 -DUNICODE -D_UNICODE -Os -s -o qiin-tabtip.exe qiin-tabtip.cpp -lshell32 -luser32 -ladvapi32 -lole32 -loleaut32 -luuid -static-libgcc -static-libstdc++ -Wl,--gc-sections -municode"
else
    echo "Error: no C++ compiler was found"
    echo ""
    echo "Install one of the following:"
    echo "  - Visual Studio (MSVC)"
    echo "  - MinGW-w64"
    echo "  - Clang"
    exit 1
fi

echo "Compiler: $COMPILER"
echo ""
echo "Building..."
echo "========================================"

# Run the compiler.
eval $COMPILE_CMD

if [ $? -eq 0 ]; then
    echo ""
    echo "========================================"
    echo "Build succeeded"
    echo "========================================"
    echo ""
    
    # Show artifact information.
    if [ -f "qiin-tabtip.exe" ]; then
        FILE_SIZE=$(stat -c%s "qiin-tabtip.exe" 2>/dev/null || stat -f%z "qiin-tabtip.exe" 2>/dev/null || echo "unknown")
        if [ "$FILE_SIZE" != "unknown" ]; then
            SIZE_KB=$((FILE_SIZE / 1024))
            echo "File: qiin-tabtip.exe"
            echo "Size: ${SIZE_KB} KB"
            echo ""
        fi
    fi
    
    echo "Usage:"
    echo "  ./qiin-tabtip.exe        # Toggle the touch keyboard"
    echo "  ./qiin-tabtip.exe show   # Show the touch keyboard"
    echo "  ./qiin-tabtip.exe osk    # Open the on-screen keyboard"
    echo "  ./qiin-tabtip.exe help   # Show help"
    echo ""
else
    echo ""
    echo "========================================"
    echo "Build failed"
    echo "========================================"
    exit 1
fi

