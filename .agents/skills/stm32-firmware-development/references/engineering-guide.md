# STM32 firmware engineering guide

Read only the sections relevant to the current request.

## Source priority

Use the source appropriate to the claim:

1. User-approved module requirements and explicit user decisions govern intended behavior and required constraints.
2. Current project source, `.ioc`, build configuration, linker map, and generated device/HAL headers govern implemented behavior.
3. The exact MCU datasheet, reference manual, and current errata govern electrical limits and peripheral behavior.
4. The matching STM32Cube HAL source and headers in the project govern HAL contracts and state transitions.
5. Official ST application notes and examples apply when the project and primary device documents do not answer the question.
6. Use clearly labeled engineering inference only when the higher-priority sources cannot resolve a non-consequential detail.

When sources conflict, distinguish intended configuration from actual generated or compiled behavior. State the conflict and ask if it changes the requested result. Do not silently transplant code from another STM32 family or HAL version.

## New firmware and feature changes

- Derive inputs, outputs, timing, overload behavior, and error recovery from the request and project.
- Choose the smallest architecture that satisfies those constraints. A direct superloop step, cooperative state machine, callback-driven module, interrupt, or DMA pipeline is a task decision, not a universal default.
- Represent time using wrap-safe elapsed-time comparisons. Avoid correctness that depends on a particular main-loop iteration rate unless that rate is measured and required.
- Bound queues, buffers, retries, and per-iteration work. Define what happens on overflow, timeout, peripheral error, and stale data.
- Keep module ownership visible. Prefer project-owned modules over large additions scattered among generated callbacks.
- Check HAL return values when failure or busy state can affect correctness. Do not blindly restart a peripheral whose HAL state or hardware flags are unknown.

## Interrupts and shared state

- Use an interrupt only when latency, event loss, wake-up behavior, or peripheral operation requires it.
- Keep ISR and HAL callback work bounded. Avoid formatting, long loops, blocking HAL calls, and unrelated application policy in interrupt context.
- Establish which context writes and reads each shared object. Consider atomic access width, read-modify-write races, ordering, and snapshot consistency.
- Use `volatile` for values that can change outside ordinary control flow when required, but address multi-field consistency and compound operations separately.
- Clear or acknowledge hardware conditions according to the exact peripheral sequence in the reference manual and HAL implementation.
- Assign priorities only after identifying nesting, latency, and any HAL time-base dependency. Do not guess a safe priority scheme.

## DMA

- Use DMA when measured or required transfer rate, CPU load, latency, or sampling regularity justifies its lifecycle complexity.
- Define buffer ownership during transfer, completion, half-completion, cancellation, and error callbacks.
- Check length units and counter widths; HAL APIs may express lengths in data items rather than bytes.
- Protect against reuse of a buffer still owned by DMA. Choose single, ping-pong, or ring buffering from producer/consumer timing.
- On this Cortex-M3 target, do not invent data-cache maintenance requirements. Re-evaluate cache coherency when porting the workflow to a cached MCU.

## Bare-metal non-blocking behavior

- Preserve the progress of operations that span time and advance them from events, elapsed time, or peripheral completion using the architecture appropriate to the module.
- Do not use `HAL_Delay`, busy loops, polling-for-completion, or blocking HAL calls in project-owned firmware paths, including module startup and initialization. Express startup progress asynchronously and enforce its required finite timeout.
- Keep every main-loop activity bounded so unrelated work continues to make progress.
- Define cancellation, timeout, retry, and recovery for operations that may never complete.

## Debugging

1. Reproduce or restate the observable failure and the conditions under which it occurs.
2. Trace the smallest relevant path across initialization, main context, callbacks, ISRs, and peripheral state.
3. Separate confirmed observations from hypotheses. Seek evidence that can falsify the leading hypothesis.
4. Check configuration/code mismatches: clocks, GPIO modes, alternate functions, DMA channels, NVIC enablement/priorities, handle instances, buffer sizes, and HAL state.
5. Implement the narrowest justified correction within the authorized regions. Do not conceal a configuration defect with an unrelated application workaround.

## Code review

Prioritize findings that can change runtime behavior:

- blocking or unbounded work;
- ISR latency, reentrancy, race conditions, and invalid `volatile` assumptions;
- buffer bounds, lifetime, aliasing, alignment, and DMA ownership;
- integer overflow, signedness, truncation, tick wraparound, and unit mismatch;
- unchecked HAL results, stuck busy states, missing timeouts, and incomplete recovery;
- stack-heavy locals, recursion, hidden dynamic allocation, and oversized static buffers;
- regeneration-unsafe edits and assumptions inconsistent with `.ioc` or device headers.

For each finding, identify the location, failure mechanism, trigger, impact, and smallest safe correction. Distinguish a correctness defect from a fragile design or optional improvement.

## Source-level RAM and Flash optimization

- Inspect source-level evidence before optimizing: static object and buffer sizes, stack-heavy call paths, call graph, generated-code dependencies, and any existing approved build evidence supplied to the task.
- Compiled section totals, linker-map or symbol measurements, generated assembly, and build-to-build comparisons belong to `$stm32-build-flash-debug`. Do not invoke the toolchain under this skill.
- Report the baseline and expected source-level effect. Label estimates as estimates, and do not trade correctness or maintainability for an unquantified saving.
- For RAM, inspect static buffers, duplicated tables, object lifetimes, stack depth, library state, and alignment padding.
- For Flash, inspect linked library features, formatting functions, duplicate constants, dead code, inlining, optimization/LTO settings, and generated HAL surface.
- Changes to compiler flags, CMake, or linker configuration require permission under this skill's edit boundary.

## Portability beyond STM32F103C8T6

Treat a future MCU change as a new hardware context. Re-confirm core architecture, memory map, clock tree, GPIO/alternate-function mapping, DMA routing, interrupt model, peripheral instances, HAL version, linker configuration, and errata. Preserve application-level interfaces where useful, but do not assume register behavior or generated initialization is portable.
