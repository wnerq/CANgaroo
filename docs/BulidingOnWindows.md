# Building Qt 6.11.2 from Source on Windows with MinGW-w64

## Purpose

This document describes how we built **Qt 6.11.2 from source on Windows without creating a Qt account**, then used that Qt installation to build **CANgaroo** as a native Windows application.

The final toolchain was:

- Windows 11
- MSYS2
- MinGW-w64 / GCC 16.2.0
- CMake 4.4.2
- Ninja 1.13.2
- Python 3.14
- Qt 6.11.2 built from source
- qmake for building CANgaroo
- pybind11 for CANgaroo's embedded Python support

No Visual Studio or MSVC compiler was required.

---

# 1. What We Were Actually Building

There are two separate builds involved:

1. **Build Qt itself**
2. **Build CANgaroo using the Qt libraries we just built**

The dependency chain looks roughly like this:

    MSYS2
      |
      +-- MinGW-w64 / GCC
      |      |
      |      +-- C/C++ compiler
      |      +-- linker
      |      +-- Windows runtime libraries
      |
      +-- CMake
      +-- Ninja
      +-- Python
             |
             v
        Qt 6.11.2 source
             |
             v
        Our Qt installation
        C:\Qt\6.11.2\mingw_64
             |
             v
        CANgaroo source
             |
             v
        cangaroo.exe

Qt itself is therefore not the compiler. Qt is primarily a large set of C++ libraries, tools, APIs, and frameworks.

We needed a C++ compiler first in order to turn the Qt source code into usable Windows libraries.

---

# 2. Why MinGW-w64?

Windows has multiple viable C++ compiler toolchains.

Two common choices for Qt are:

- Microsoft Visual C++ (MSVC)
- MinGW-w64/GCC

We chose **MinGW-w64** because it allowed us to create native Windows programs without installing Visual Studio.

MinGW-w64 provides Windows versions of the GNU compiler tools, including:

    gcc.exe
    g++.exe
    gdb.exe
    ar.exe
    windres.exe

`gcc` is primarily the C compiler.

`g++` is the C++ compiler.

Both generate genuine native Windows executables and DLLs. We confirmed this early in the process by compiling a small program and examining the result:

    PE32+ executable for MS Windows ... x86-64

This is important: **MinGW is not WSL**.

The resulting programs are normal Windows `.exe` files.

---

# 3. Why MSYS2?

MinGW-w64 is the compiler toolchain, while **MSYS2** provides a convenient Windows development environment and package manager around it.

MSYS2 gave us:

- MinGW-w64
- GCC/G++
- GDB
- CMake
- Ninja
- Python
- pybind11
- Various runtime libraries
- `pacman` package management

This makes maintaining all the related dependencies considerably easier than manually downloading each tool.

We installed MSYS2 from Windows PowerShell with:

    winget install --id MSYS2.MSYS2 -e

---

# 4. MSYS2 Environments

MSYS2 provides several different shells.

We used:

    MSYS2 UCRT64

This matters.

Inside that shell:

    echo $MSYSTEM

should report:

    UCRT64

The UCRT64 environment uses Microsoft's modern Universal C Runtime while compiling our application with GCC/MinGW.

The compiler and tools consequently lived under:

    C:\msys64\ucrt64\bin

which appears inside MSYS2 as:

    /ucrt64/bin

For example:

    /ucrt64/bin/gcc
    /ucrt64/bin/g++
    /ucrt64/bin/cmake
    /ucrt64/bin/ninja
    /ucrt64/bin/python

We verified this with:

    which gcc g++ cmake ninja python

---

# 5. Updating MSYS2

Before installing the development toolchain, we updated MSYS2:

    pacman -Syu

If MSYS2 requests that the terminal be restarted during this process, close it, reopen **MSYS2 UCRT64**, and run the command again.

---

# 6. Installing MinGW-w64

We installed the UCRT64 MinGW development toolchain:

    pacman -S mingw-w64-ucrt-x86_64-toolchain

When asked which packages from the toolchain group to install, we accepted the default of all packages.

We then verified the installation:

    gcc --version
    g++ --version
    gdb --version

Our resulting versions were:

    GCC/G++ 16.2.0
    GDB 17.2

---

# 7. Verifying Native Windows Compilation

Before involving Qt, we verified that the compiler itself worked.

We created:

    hello.cpp

containing:

    #include <iostream>

    int main()
    {
        std::cout << "Hello from MinGW-w64!" << std::endl;
        return 0;
    }

We compiled it:

    g++ hello.cpp -o hello.exe

and ran it:

    ./hello.exe

which produced:

    Hello from MinGW-w64!

We then examined the executable:

    file hello.exe

The result identified it as:

    PE32+ executable for MS Windows ... x86-64

This established that the MinGW toolchain was producing genuine 64-bit Windows executables before we introduced Qt into the equation.

---

# 8. Installing the Qt Build Tools

Qt itself has a large and sophisticated build system.

We installed:

    pacman -S mingw-w64-ucrt-x86_64-cmake \
              mingw-w64-ucrt-x86_64-ninja \
              mingw-w64-ucrt-x86_64-python

We then verified:

    cmake --version
    ninja --version
    python --version

Our versions were:

    CMake 4.4.2
    Ninja 1.13.2
    Python 3.14.7

## What these tools do

### CMake

CMake is a **build configuration/generation system**.

It examines the project, compiler, operating system, dependencies, and configuration options and generates instructions for another build tool.

It does not normally do the actual compilation itself.

### Ninja

Ninja is the **build executor**.

CMake generated Ninja build files, and Ninja coordinated the thousands of individual compiler and linker operations required to build Qt.

### Python

Parts of Qt's build system and code-generation infrastructure use Python scripts.

---

# 9. Downloading Qt Source

We created:

    C:\src

which appears in MSYS2 as:

    /c/src

Then downloaded the official Qt 6.11.2 source archive:

    curl -LO https://download.qt.io/official_releases/qt/6.11/6.11.2/single/qt-everywhere-src-6.11.2.tar.xz

The archive was approximately 973 MB.

We verified the SHA-256 checksum:

    sha256sum qt-everywhere-src-6.11.2.tar.xz

Expected:

    6dcfbca271d76a6502741a2c0dc6fc98ef7dd0b7b4cfd0abcebb285a86a26f33

Our checksum matched.

This verifies that the downloaded archive was not corrupted or modified.

---

# 10. Extracting Qt

We extracted the archive:

    tar -xf qt-everywhere-src-6.11.2.tar.xz

This created:

    C:\src\qt-everywhere-src-6.11.2

We intentionally kept the source tree separate from the build tree.

---

# 11. Source, Build, and Install Directories

We used three separate locations:

    C:\src\qt-everywhere-src-6.11.2

        Original Qt source code

    C:\build\qt-6.11.2

        Temporary/intermediate build files

    C:\Qt\6.11.2\mingw_64

        Final installed Qt libraries and tools

This separation is important.

The source directory remains relatively clean, while the build directory can be completely deleted if we need to perform a clean rebuild.

The final installation contains only the files needed to use Qt after it has been built.

---

# 12. Configuring the Qt Build

From:

    /c/build/qt-6.11.2

we initially ran:

    /c/src/qt-everywhere-src-6.11.2/configure \
        -prefix C:/Qt/6.11.2/mingw_64 \
        -release \
        -opensource \
        -confirm-license \
        -nomake examples \
        -nomake tests

Important options:

### `-prefix`

Specifies where the finished Qt installation should be placed:

    C:\Qt\6.11.2\mingw_64

### `-release`

Builds the optimized release configuration.

### `-opensource`

Selects the open-source Qt licensing configuration.

### `-confirm-license`

Confirms the selected license non-interactively.

### `-nomake examples`

Does not build Qt's large collection of example applications.

### `-nomake tests`

Does not build Qt's own test suites.

Neither examples nor tests were necessary for our purpose.

---

# 13. What `configure` Actually Did

`configure` did not build Qt.

It examined the machine and determined things such as:

- Which compiler was available
- Compiler capabilities
- Windows platform
- Available libraries
- Qt modules that could be built
- Missing optional dependencies
- CMake configuration
- Ninja configuration
- Installation paths

It then generated the actual build system.

Our configuration correctly detected:

    C compiler:   GNU 16.2.0
    C++ compiler: GNU 16.2.0
    Generator:    Ninja

At completion, Qt reported:

    Qt is now configured for building.

---

# 14. Error: Out of Memory During Qt Build

Our first build attempt was:

    cmake --build . --parallel

This allowed CMake/Ninja to choose a large number of simultaneous compiler jobs.

The build failed with:

    cc1.exe: out of memory allocating ...

This was not a source-code error.

Too many instances of the compiler were consuming memory simultaneously.

## Solution

We limited the number of parallel build jobs:

    cmake --build . --parallel 4

If necessary, this could be reduced further:

    cmake --build . --parallel 2

Ninja can resume an interrupted build, so a complete restart is normally unnecessary after this type of failure.

---

# 15. Error: Qt WebEngine Failed to Build

The next build proceeded much farther but eventually failed inside:

    qtwebengine

with errors involving Qt's bundled GN build tool, including:

    error: 'int64_t' does not name a type

However, the more important clue had appeared during configuration:

    QtWebEngine won't be built.
    GNU compiler is not supported.

Qt WebEngine is based heavily on Chromium and does not support our MinGW configuration.

CANgaroo does not require Qt WebEngine, so there was no reason to solve the WebEngine build problem.

## Solution

We deleted the Qt build directory and configured again:

    cd /c/build
    rm -rf qt-6.11.2
    mkdir qt-6.11.2
    cd qt-6.11.2

Then explicitly excluded Qt WebEngine:

    /c/src/qt-everywhere-src-6.11.2/configure \
        -prefix C:/Qt/6.11.2/mingw_64 \
        -release \
        -opensource \
        -confirm-license \
        -nomake examples \
        -nomake tests \
        -skip qtwebengine

Configuration then explicitly reported that WebEngine was disabled.

---

# 16. Building Qt

We built Qt with constrained parallelism:

    cmake --build . --parallel 4

The build contained roughly 13,700 individual build operations.

Eventually Ninja reached:

    [13716/13716]

with no error.

This meant Qt had successfully compiled.

---

# 17. Compiler Warnings

During the Qt build we saw warnings such as:

    warning: defining 'QChar', which previously failed to be complete
    in a SFINAE context

These appeared because we were compiling Qt 6.11.2 using the relatively new GCC 16.2 compiler.

They were warnings, not errors.

The build continued successfully, so we did not attempt to modify Qt source code to eliminate them.

A useful distinction during builds is:

    warning:

usually means compilation can continue.

Whereas:

    error:
    fatal error:
    FAILED:
    undefined reference:

usually indicates something that must be fixed.

---

# 18. Installing Qt

Building Qt populated the build directory but did not yet create the clean final installation.

We installed the finished build with:

    cmake --install .

This populated:

    C:\Qt\6.11.2\mingw_64

We verified the installation:

    /c/Qt/6.11.2/mingw_64/bin/qmake.exe -query QT_VERSION

which returned:

    6.11.2

At this point we had a functioning self-built Qt installation.

---

# 19. Qt Smoke Test

Before attempting CANgaroo, we created a minimal Qt application.

The application created a `QApplication` and displayed a `QLabel` containing:

    Qt 6.11.2 + MinGW works

Its CMake configuration used:

    find_package(Qt6 REQUIRED COMPONENTS Widgets)

and:

    target_link_libraries(QtSmokeTest PRIVATE Qt6::Widgets)

We configured the application using our newly built Qt:

    /c/Qt/6.11.2/mingw_64/bin/qt-cmake.bat \
        -G Ninja \
        -S . \
        -B build

Then:

    cmake --build build

The application compiled successfully.

---

# 20. Console Window Behind the Qt Application

Initially the Qt smoke test opened both:

- The Qt GUI
- A Windows console

This happened because the executable was built as a console-subsystem Windows application.

## Solution

We changed:

    qt_add_executable(QtSmokeTest
        main.cpp
    )

to:

    qt_add_executable(QtSmokeTest WIN32
        main.cpp
    )

The `WIN32` option causes CMake/Qt to create a Windows GUI-subsystem executable.

After rebuilding, the unnecessary console disappeared.

---

# 21. Error: Missing MinGW Runtime DLLs

Launching the smoke-test executable directly from Windows Explorer initially failed with:

    libstdc++-6.dll was not found

This occurred because the executable was compiled using GCC.

The GCC C++ runtime exists inside MSYS2, so programs launched from that environment could find it. Windows Explorer, however, did not know where the MSYS2 runtime libraries were located.

The important runtime DLLs included:

    libstdc++-6.dll
    libgcc_s_seh-1.dll
    libwinpthread-1.dll

We copied them from:

    C:\msys64\ucrt64\bin

into the directory containing the executable.

---

# 22. Error: Missing Qt6Core.dll

The smoke test also reported:

    Qt6Core.dll was not found

Our executable dynamically links against Qt.

That means the executable itself does not contain Qt. It expects the required Qt DLLs to be available at runtime.

## Solution: windeployqt

Qt supplies a deployment utility:

    windeployqt.exe

We ran:

    /c/Qt/6.11.2/mingw_64/bin/windeployqt.exe \
        /c/build/qt-smoketest/build/QtSmokeTest.exe

This copied the appropriate Qt DLLs and plugins beside the application.

These included things such as:

    Qt6Core.dll
    Qt6Gui.dll
    Qt6Widgets.dll

and Qt's Windows platform plugin:

    platforms/qwindows.dll

---

# 23. Error: Additional Missing Runtime DLLs

After deploying Qt, Windows reported additional missing DLLs:

    libb2-1.dll
    libpcre2-16-0.dll
    zlib1.dll

We used:

    ldd QtSmokeTest.exe

and:

    ldd Qt6Core.dll

to inspect runtime dependencies.

This revealed additional libraries being loaded from:

    /ucrt64/bin

including:

    libpcre2-16-0.dll
    libb2-1.dll
    zlib1.dll
    libzstd.dll

We copied those beside the executable as well.

After doing so, the smoke test successfully launched directly from Windows Explorer.

This proved that:

- MinGW worked
- Qt worked
- Qt Widgets worked
- Dynamic linking worked
- Qt's Windows platform plugin worked
- The resulting application was a normal native Windows GUI application

---

# 24. Building CANgaroo

Initially we attempted to configure CANgaroo using CMake.

This failed with:

    The source directory does not appear to contain CMakeLists.txt.

Examining the CANgaroo repository showed:

    cangaroo.pro

but no:

    CMakeLists.txt

This means CANgaroo uses Qt's older **qmake** build system rather than CMake.

---

# 25. qmake vs. CMake

Our Qt installation supports both approaches.

For modern CMake applications we can use:

    qt-cmake.bat

For CANgaroo, however, we needed:

    qmake.exe

The `.pro` file performs roughly the same conceptual role for qmake that `CMakeLists.txt` performs for CMake.

---

# 26. Configuring CANgaroo with Our Qt Installation

We created a separate build directory:

    C:\git\wnerq_CANgaroo\build-mingw

Then ran our Qt installation's qmake:

    /c/Qt/6.11.2/mingw_64/bin/qmake.exe ../cangaroo.pro

This generated Makefiles configured for:

- Our Qt 6.11.2 installation
- MinGW
- The Windows platform

We then built with:

    mingw32-make

---

# 27. Error: Missing pybind11

CANgaroo compilation stopped with:

    fatal error: pybind11/embed.h: No such file or directory

CANgaroo contains functionality that embeds Python using **pybind11**.

The compiler therefore needed the pybind11 C++ header files.

## Solution

We installed the UCRT64 version:

    pacman -S mingw-w64-ucrt-x86_64-pybind11

We verified:

    /ucrt64/include/pybind11/embed.h

existed.

Then we simply resumed:

    mingw32-make

There was no need to rebuild Qt.

---

# 28. CANgaroo Build Success

Eventually the linker produced:

    cangaroo.exe

under:

    C:\git\wnerq_CANgaroo\build-mingw\bin

The final linker command also revealed several important dependencies, including:

    Qt6Charts
    Qt6OpenGLWidgets
    Qt6Widgets
    Qt6OpenGL
    Qt6Svg
    Qt6Gui
    Qt6Xml
    Qt6SerialBus
    Qt6SerialPort
    Qt6Network
    Qt6Core
    Python 3.14

This confirms why building a fairly complete Qt installation was useful.

---

# 29. Deploying CANgaroo

As with the smoke test, building the executable does not automatically make its directory self-contained.

We ran `windeployqt` against CANgaroo to deploy its Qt dependencies.

We also copied the required MinGW/MSYS2 runtime libraries.

Then we examined CANgaroo with:

    ldd /c/git/wnerq_CANgaroo/build-mingw/bin/cangaroo.exe

This showed which dependencies were:

- Native Windows system DLLs
- Already present beside CANgaroo
- Still being loaded from MSYS2

Most Windows DLLs such as:

    KERNEL32.DLL
    USER32.dll
    SETUPAPI.dll
    WINUSB.DLL
    WS2_32.dll

are supplied by Windows and should **not** be copied from the Windows system directory.

---

# 30. Additional CANgaroo Runtime Dependencies

`ldd` identified three remaining libraries coming from `/ucrt64/bin`:

    libpython3.14.dll
    libbrotlidec.dll
    libbrotlicommon.dll

These were copied into:

    C:\git\wnerq_CANgaroo\build-mingw\bin

After doing so, CANgaroo successfully launched as a native Windows application.

---

# 31. WSL vs. MSYS2 Confusion

At one point we attempted to run:

    /c/Qt/...

from a shell whose prompt looked like:

    user@computer:/mnt/c/...

That was WSL.

WSL exposes the Windows C: drive as:

    /mnt/c

MSYS2 exposes it as:

    /c

More importantly, we were building a **native Windows MinGW application**, so we needed the MSYS2 UCRT64 environment rather than WSL.

A useful check is:

    echo $MSYSTEM

For this build it should report:

    UCRT64

---

# 32. Final Directory Layout

The important directories ended up approximately as follows:

    C:\
    |
    +-- msys64\
    |   +-- ucrt64\
    |       +-- bin\
    |       +-- include\
    |       +-- lib\
    |
    +-- src\
    |   +-- qt-everywhere-src-6.11.2\
    |
    +-- build\
    |   +-- qt-6.11.2\
    |   +-- qt-smoketest\
    |
    +-- Qt\
    |   +-- 6.11.2\
    |       +-- mingw_64\
    |           +-- bin\
    |           +-- include\
    |           +-- lib\
    |           +-- plugins\
    |
    +-- git\
        +-- wnerq_CANgaroo\
            +-- src\
            +-- cangaroo.pro
            +-- build-mingw\
                +-- bin\
                    +-- cangaroo.exe

Conceptually:

    C:\src       = source code we downloaded
    C:\build     = disposable Qt build workspace
    C:\Qt        = finished reusable Qt installation
    C:\git       = application source repositories

---

# 33. The Complete Process in Condensed Form

The entire process can be summarized as:

1. Install MSYS2.
2. Open the MSYS2 UCRT64 environment.
3. Update MSYS2.
4. Install the MinGW-w64 toolchain.
5. Verify GCC can produce a native Windows executable.
6. Install CMake, Ninja, and Python.
7. Download the Qt source archive.
8. Verify the Qt archive checksum.
9. Extract the Qt source.
10. Create a separate Qt build directory.
11. Configure Qt for MinGW.
12. Explicitly disable unsupported Qt WebEngine.
13. Build Qt using constrained parallelism.
14. Install Qt into `C:\Qt\6.11.2\mingw_64`.
15. Build a small Qt Widgets smoke test.
16. Deploy its Qt DLLs with `windeployqt`.
17. Copy required MinGW/MSYS2 runtime DLLs.
18. Confirm the Qt GUI launches directly from Windows.
19. Determine that CANgaroo uses qmake rather than CMake.
20. Configure CANgaroo using our Qt installation's `qmake.exe`.
21. Install pybind11 when CANgaroo reported it missing.
22. Build CANgaroo using `mingw32-make`.
23. Deploy CANgaroo's Qt dependencies using `windeployqt`.
24. Use `ldd` to identify remaining non-Windows runtime dependencies.
25. Copy those runtime DLLs beside CANgaroo.
26. Launch CANgaroo successfully as a native Windows application.

---

# 34. Important Lessons

## Building and installing are different operations

For Qt:

    configure
        ↓
    build
        ↓
    install

Each has a distinct purpose.

**Configure** determines how the software should be built.

**Build** invokes the compiler/linker to create the software.

**Install** copies the resulting libraries, headers, executables, plugins, and metadata into a clean reusable installation.

---

## Compile-time and runtime dependencies are different

Successfully creating:

    cangaroo.exe

does not mean Windows has everything necessary to run it.

At compile/link time, the build system knew where Qt, Python, and MinGW were installed.

When launching an executable from Explorer, Windows needs to locate the corresponding runtime DLLs independently.

That is why the program could compile successfully but subsequently report:

    Qt6Core.dll was not found

or:

    libstdc++-6.dll was not found

---

## `windeployqt` handles Qt, but not necessarily everything

`windeployqt` is extremely useful because it understands Qt's DLL and plugin structure.

However, our application also depended on libraries from the MSYS2/MinGW environment.

We therefore still needed to inspect dependencies with:

    ldd

and copy the appropriate non-system runtime DLLs.

---

## Do not blindly copy Windows system DLLs

Libraries such as:

    KERNEL32.DLL
    USER32.dll
    GDI32.dll
    WS2_32.dll

are Windows components.

They should normally remain supplied by Windows.

The libraries we needed to deploy ourselves were things associated with our compiler, Qt installation, Python, and third-party libraries.

---

## Use the same compiler ecosystem consistently

Our stack is:

    MinGW-built Qt
          +
    MinGW-built CANgaroo

This is important because C++ compilers have ABI differences.

Mixing an MSVC-built Qt installation with a MinGW-built application is generally not viable.

---

## The MSYS2 shell is part of the development environment, not the final application

We used MSYS2 to provide:

- GCC
- build tools
- headers
- libraries
- Unix-like shell utilities

But the final result is not an MSYS2 application in the same sense that a WSL executable is a Linux application.

`cangaroo.exe` is a native Windows executable.

Once all required runtime DLLs are deployed beside it, Windows can launch it directly from Explorer without opening MSYS2.

---

# 35. Result

We ultimately produced a working native Windows build of CANgaroo using:

    Qt 6.11.2
        built from source
             +
    MinGW-w64 / GCC 16.2
             +
    MSYS2 UCRT64
             +
    qmake / mingw32-make

The Qt installation resides at:

    C:\Qt\6.11.2\mingw_64

and CANgaroo was built under:

    C:\git\wnerq_CANgaroo\build-mingw

This achieved the original objective of obtaining and using Qt on Windows **without using the Qt Online Installer, without creating a Qt account, and without requiring Visual Studio/MSVC**.