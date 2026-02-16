# Native Test Environment Setup (Windows)

This document describes how to set up the native unit test environment
for the ESP32 Firmware Core project.

The native environment allows domain modules to be tested on the host
machine without ESP-IDF or hardware dependencies.

------------------------------------------------------------------------

## 1. Prerequisites

Install the following tools:

-   Python (3.10+ recommended)
-   Git
-   PlatformIO CLI
-   MSYS2

Verify PlatformIO installation:

    pio --version

------------------------------------------------------------------------

## 2. Install MSYS2

Download and install MSYS2 from:

https://www.msys2.org/

After installation:

1.  Open **MSYS2 UCRT64** shell.
2.  Update the system:

```{=html}
<!-- -->
```
    pacman -Syu

Restart the MSYS2 shell if required and run again:

    pacman -Syu

------------------------------------------------------------------------

## 3. Install GCC (UCRT64 Toolchain)

Inside the MSYS2 UCRT64 shell:

    pacman -S mingw-w64-ucrt-x86_64-gcc

This installs the GCC toolchain required by PlatformIO's native
environment.

------------------------------------------------------------------------

## 4. Add GCC to Windows PATH

Add the following directory to your Windows PATH:

    C:\msys64\ucrt64\bin

After updating PATH:

1.  Close all terminals.
2.  Open a new PowerShell or CMD.
3.  Verify installation:

```{=html}
<!-- -->
```
    gcc --version

You should see the MSYS2 GCC version output.

------------------------------------------------------------------------

## 5. PlatformIO Native Environment Configuration

Ensure the following configuration exists in `platformio.ini`:

    [env:native]
    platform = native
    test_framework = unity
    build_type = debug
    test_build_src = no
    build_flags =
      -std=c11

Important:

-   `test_build_src = no` prevents firmware (ESP-IDF) code from being
    compiled during native tests.
-   Only test files and libraries in `/lib` are compiled.

------------------------------------------------------------------------

## 6. Running Native Unit Tests

From the project root:

    pio test -e native

Expected result:

-   All tests executed
-   0 failures
-   Deterministic output

------------------------------------------------------------------------

## 7. Common Issues and Solutions

### gcc not found

Cause: GCC is not in PATH.

Solution: Ensure `C:\msys64\ucrt64\bin` is added to PATH and open a new
terminal.

------------------------------------------------------------------------

### undefined reference to WinMain

Cause: No `main()` function present in native environment.

Solution: Ensure `test_main.c` provides a Unity test runner:

    int main(void)
    {
        UNITY_BEGIN();
        ...
        return UNITY_END();
    }

------------------------------------------------------------------------

### FreeRTOS or ESP-IDF headers not found

Cause: Firmware sources are being compiled in native environment.

Solution: Ensure in `[env:native]`:

    test_build_src = no

------------------------------------------------------------------------

## 8. Clean Build (If Needed)

If build artifacts cause issues:

    pio run -t clean -e native
    pio test -e native -v

------------------------------------------------------------------------

Environment setup complete.
