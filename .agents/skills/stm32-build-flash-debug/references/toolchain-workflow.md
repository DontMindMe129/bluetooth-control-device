# STM32 toolchain workflow

Read only the branch required by the request. Project-defined commands take precedence over the generic command shapes below.

## Discovery

Inspect, when present:

- `CMakePresets.json` and `CMakeUserPresets.json`;
- top-level and relevant nested `CMakeLists.txt` files;
- the Arm GCC toolchain file and linker script;
- existing build, flash, debug, launch, OpenOCD, or ST-LINK configuration;
- `.ioc`, generated device headers, and startup vector for target identity;
- prior build cache only when it belongs to the current workspace and configuration.

Do not assume the name or location of a build directory. Do not select a different toolchain or regenerate CubeMX output without authorization.

## Configure and build

Before each configure or build command, state the concrete command, build directory/configuration, and purpose, then obtain explicit user approval. Do not run the command until approval is received. Prefer a checked-in preset when available:

```text
cmake --preset <preset>
cmake --build --preset <build-preset>
```

Otherwise derive arguments from project documentation/configuration; a common shape is:

```text
cmake -S <source-dir> -B <build-dir> -G Ninja -DCMAKE_TOOLCHAIN_FILE=<toolchain-file> -DCMAKE_BUILD_TYPE=<configuration>
cmake --build <build-dir>
```

Do not present these placeholders as project commands. Record the concrete command actually used.

On failure, preserve the earliest causal diagnostic rather than focusing on cascaded errors. Check compiler discovery and version, CMake cache/toolchain consistency, include paths and defines, generated sources, startup/device selection, linker script, missing symbols, section overflow, and post-build utilities according to the failure phase.

Do not clean first: stale state is a hypothesis to prove, and cleaning is a separate destructive operation under this workflow.

## Size and map inspection

Prefer the toolchain's size utility and linker map/symbol evidence, using the executable names available in the installed Arm GNU Toolchain. Confirm which sections contribute to nonvolatile load and runtime RAM.

- Flash commonly includes code, read-only data, and initial values stored for writable data.
- Runtime RAM commonly includes writable data, zero-initialized data, retained/no-init areas, reserved heap/stack, and dynamic call-stack demand.
- An ELF file contains metadata and is not itself a Flash-usage measurement.

For optimization comparisons, keep toolchain, configuration, flags, and input revision constant. Report before/after totals and relevant symbol or section deltas.

## Flash

Flash only after explicit authorization and a pre-action notice naming the target/probe, artifact, tool, and expected reset/run behavior.

Use the project's configured programming software with the confirmed ST-Link V2 probe. Do not substitute another probe or programming setup without user approval. Before programming, confirm:

- the artifact belongs to the current successful build;
- device and memory layout match the project;
- probe selection is unambiguous;
- the command does not mass-erase or change option bytes unexpectedly;
- verification and post-program reset/run behavior are understood.

Stop on a device-ID mismatch, protection state, ambiguous probe, unstable connection, or unexpected erase request. Report the state and request direction rather than looping retries.

## Hardware debug

Starting a debug server, connecting GDB, halting, resetting, or changing execution requires explicit authorization and notice. Use the STM32Cube ST-Link GDB Server interface in VS Code and checked-in launch/debug configuration when available; ask before substituting another debug setup.

Capture the minimum evidence needed for the question: fault registers and stacked frame for exceptions, peripheral/HAL state for driver stalls, relevant memory or watch values for data issues, and a bounded trace of breakpoint behavior. Warn when breakpoints or halting can change timing-sensitive peripheral behavior.

Do not write arbitrary memory, patch Flash, modify option bytes, or continue execution from a corrupted state without a separately justified and authorized action. End by reporting whether the target is running, halted, reset, disconnected, or unknown.
