---
name: stm32-build-flash-debug
description: Configure and build CubeMX-generated STM32F103C8T6 firmware, diagnose compiler/linker failures, inspect compiled size and map evidence, or operate ST-Link V2 flash and on-target debug workflows. Require explicit authorization before every configure, build, flash, or hardware-debug action.
---

# STM32 Build, Flash, and Debug

Operate the STM32 toolchain with visible commands, verified targets, and clear separation between local builds and actions that affect hardware.

## Establish the toolchain

1. Inspect the relevant `CMakeLists.txt`, presets or toolchain files, existing build documentation, `.ioc`, linker script, and debug configuration.
2. Confirm the exact MCU, build configuration, artifact, build directory, Arm GNU Toolchain, Ninja, and available ST-Link-compatible program/debug software from project evidence.
3. Prefer existing project commands and configuration. Do not invent a flash address, linker script, reset mode, debug server, or probe serial number.
4. If multiple boards or probes are connected, or the target/tool is ambiguous, stop and ask the user to identify it.

Read [references/toolchain-workflow.md](references/toolchain-workflow.md) for the relevant build, diagnostic, size, flash, or debug branch.

## Notification and authorization

- Every configure or build invocation requires explicit user approval immediately before it runs. State the command, build directory/configuration, and purpose, then wait for approval. Do not treat a request that includes building as permission to skip this approval step.
- Flashing, erasing, resetting, halting, or starting a hardware-debug session requires an explicit user request or explicit acceptance of the proposed action. Before acting, state the target/probe, artifact, tool, and expected target-state change.
- Treat a prior build request as permission to build only, not permission to flash or debug.
- Do not modify CMake files, linker scripts, startup files, generated text outside CubeMX user regions, or debug configuration without explaining the proposed edit and obtaining permission.
- Build output may be created in the project's established build directory. Do not delete or clean build directories unless the user explicitly requests it.

## Build and diagnose

Use the established configure/build workflow and preserve the first actionable diagnostic. Classify failures as configuration, compilation, linking, size overflow, or tool/environment problems before proposing a change.

When a source fix is needed, use `$stm32-firmware-development` for edits inside its boundary. Do not alter code or configuration merely to silence a diagnostic without establishing the cause.

For RAM/Flash work, inspect section totals and the linker map or symbols when available. Distinguish Flash image size, load memory, runtime RAM, heap reservation, and stack reservation; do not report the ELF file's filesystem size as MCU usage.

## Flash or debug

After explicit authorization, verify that the artifact is current and built for the confirmed device. Use the project's configured software with the confirmed ST-Link V2 probe; use the STM32Cube ST-Link GDB Server interface in VS Code for hardware debug unless the user approves another setup. Preserve logs needed to establish connect, erase/program, verification, reset, halt, breakpoint, and fault state.

Stop rather than retrying repeatedly when the device identity, voltage/connectivity, readout protection, flash layout, or effect of an erase is unclear. Do not change option bytes, protection, or security state without a separate explicit request that names the intended change.

## Learning depth

Use **hybrid** mode by default: perform the requested workflow, then explain one to three consequential toolchain or hardware-debug facts. Use terse **delivery** mode when asked for speed and expanded **learning** mode when asked to teach. Keep explanations tied to the actual command, diagnostic, binary, or target state.

## Report

Return:

- commands actually run and their outcomes;
- build configuration and produced artifact;
- the first root-cause diagnostic and any correction made or proposed;
- RAM/Flash evidence when requested;
- for hardware operations, target/probe identity and final target state;
- skipped checks, assumptions, and any action awaiting authorization.
