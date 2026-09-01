---
name: stm32-firmware-development
description: Develop, review, optimize, and source-debug embedded C firmware for CubeMX-generated STM32F103C8T6 bare-metal projects using STM32 HAL. Use for application logic, peripheral drivers, interrupts, DMA, non-blocking behavior, and pre-build memory design; do not use for configure/build, compiler/linker or map diagnostics, flash, or on-target debug.
---

# STM32 Firmware Development

Produce maintainable firmware changes that preserve CubeMX regeneration and fit the actual timing and resource constraints of the task.

## Establish context

1. Inspect only the relevant project sources, `.ioc`, device headers, startup code, linker configuration, and generated HAL configuration needed for the request.
2. Confirm the exact MCU from project evidence. Do not infer a Blue Pill board, pinout, clock tree, memory size, or peripheral mapping from `STM32F103C8T6` alone.
3. Identify the requested outcome, observable constraints, affected execution contexts, and available validation before choosing an architecture.
4. Use user-approved module requirements for intended behavior, current source/configuration for implemented behavior, and matching ST documentation for hardware behavior. Use the HAL version and generated handles already present in the project.

If the relevant `.ioc`, peripheral configuration, timing requirement, or hardware behavior is unavailable and materially changes the implementation, ask the minimum focused question instead of inventing it.

## Change boundaries

- In CubeMX-generated files, edit only inside matching `USER CODE BEGIN` / `USER CODE END` regions by default. Keep each edit in a region that survives regeneration.
- New project-owned `.c` and `.h` modules may be created when they make the requested change clearer or safer.
- Before changing generated text outside user regions, CMake files, linker scripts, startup files, or other project configuration, explain the required change and obtain explicit permission.
- Do not allocate memory dynamically.
- Keep project-owned firmware paths non-blocking. Do not introduce busy waits, CPU polling for peripheral completion, blocking HAL calls, or delay-based coordination.
- Do not broaden the task merely because review reveals an adjacent improvement. Report it separately unless it is required for correctness.

## Engineer the change

Select the design from the task's latency, throughput, event rate, data lifetime, concurrency, failure behavior, and RAM/Flash budget. Cooperative superloop steps, state machines, callbacks, interrupts, and DMA are available choices; do not impose one without evidence that it fits. A polling-for-completion architecture is not an allowed choice.

For detailed implementation, debugging, review, interrupt/DMA, and optimization criteria, read [references/engineering-guide.md](references/engineering-guide.md) only for the relevant task branch.

When execution contexts share state, establish ownership and synchronization explicitly. Keep ISRs bounded; defer work when ISR execution is not required by latency or peripheral correctness. Use `volatile` only for visibility where appropriate, not as a substitute for atomicity or synchronization.

## Learning depth

Infer the requested mode from the prompt and allow the user to change it at any time:

- **Delivery:** implement or review directly and explain only consequential decisions.
- **Learning:** explain the relevant STM32/C mechanism, design choice, and alternative before or alongside the result.
- **Hybrid (default):** complete the task first, then explain one to three non-obvious decisions that offer the most learning value.

Do not turn routine work into a tutorial. In learning mode, connect explanations to the current code and hardware rather than presenting unrelated theory.

## Validate and report

Perform the strongest source-level checks available without operating the build, flash, or hardware-debug workflow. Trace affected callers, callback/ISR interactions, buffer bounds, timeouts, integer widths, ownership, and error paths in proportion to risk.

If compilation, compiler/linker diagnosis, or compiled size/map evidence is needed, hand off to `$stm32-build-flash-debug`; that workflow must obtain explicit user approval before configure or build. Never flash or start a hardware-debug session under this skill.

Report:

- files and behavior changed;
- assumptions or unresolved hardware facts;
- validation performed and validation not performed;
- any issue found outside the authorized edit scope;
- a concise rationale matching the selected learning mode.
