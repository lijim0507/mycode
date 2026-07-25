# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project nature

This is **not a buildable product**. It is a personal collection of reusable embedded driver
modules (mostly targeting ESP-IDF / ESP32 FreeRTOS, with STM32 and Linux ports for some
modules). Each module is an independent, copy-pastable snippet meant to be lifted into a real
project later. Treat it as a library of code fragments, not an application.

**Do not attempt to compile, link, flash, or run the code.** There is no test suite. A module
"working" means it is consistent with its own interface and the architecture rules below — not
that it builds.

Authoritative guides to read before making non-trivial changes:
- `MODULE_DESIGN.md` — the three-layer architecture, driver-injection / transport / adapter /
  handle / poll / callback / service-table patterns, and how to choose between them.
- `CODING_STYLE.md` — naming, section layout, typedef/brace rules.
- `AGENTS.md` — AI-assistant constraints and the layering rules summarized below.

When `AGENTS.md`, `CODING_STYLE.md`, or `MODULE_DESIGN.md` disagree with this file, those three
win — this file is a quick index, they are the spec.

## Module architecture (three layers)

Every module lives under `modules/<module>/` with:

```
include/   public API, types, handle/config typedefs, the driver struct typedef
core/      hardware-independent logic: state machines, protocol, algorithms
port/      platform glue; one file per platform (esp32_*, stm32_*, linux_*)
```

Core **never** calls platform peripherals directly. All hardware access goes through a
function-pointer `xxx_port_driver_t` (or `xxx_driver_t`) defined in `include/` or
`port/xxx_port.h`, injected at init. Each `port/<platform>_xxx_port.c` implements that struct
and exposes it via `xxx_port_get_driver()` (returns a pointer to a `static const` table).

Example chain (UDS over ISO-TP over CAN): `uds` → `isotp` → CAN port driver → ESP32 TWAI /
STM32 CAN. Upper modules depend on lower modules' abstract capabilities, never on concrete
hardware.

### Port init rule (important, often violated by mistake)

A port's `init` function has signature `int (*init)(void)` — **no config parameter**. All
hardware params (GPIOs, peripheral handles, etc.) are self-contained in the port `.c` file via
`#ifndef ... #define` macros at the top, overridable by compile options. Only the **core's**
`xxx_init()` may take a config, and that config carries module/logic settings only, never raw
hardware params. Callers (main.c, upper modules) must not know port hardware details.

Legacy modules still use `void *port_cfg` — leave them as-is; do not migrate unless asked.

## Module list

All modules live flat under `modules/`, each following the three-layer structure above:
`at`, `ble`, `bq40z80`, `can_diag` (containing `isotp` and `uds`), `can_simple`, `eeprom`,
`foc`, `imu`, `key`, `log-lib`, `mqtt`, `swi2c`, `uart`, `udisk`, `utility`, `ws2812`.

`modules/main.c` and `modules/main.h` are **not** a module — they are reference code for the
service-table pattern (`semaphore_operate` / `queue_operate` + FreeRTOS CPU monitor). They
reference handles that don't all exist; treat them as a wiring template, not live code.

There is no build system, no CMakeLists.txt, no partition table. Each module is meant to be
copied into a real project individually. Building (if ever needed for a real project) is via
ESP-IDF: `idf.py build` / `idf.py -p <PORT> flash monitor`. A `.devcontainer/` provides an
ESP-IDF QEMU image, and `.vscode/` sets `idf.pythonInstallPath`.

## Editing rules

- **Read the file before editing it.** Match the existing local style of that file; do not
  reformat an old file to "fix" section spacing or naming inconsistencies.
- **Prefer Edit over Write** — make targeted string replacements, don't rewrite whole files.
- **One module per task.** Don't cross-module refactor unless the user explicitly asks.
- **New code is `snake_case`.** Types are `typedef`'d with `_t`. Enums are `UPPER_CASE` with a
  module prefix. Static module globals use the `g_` prefix. Braces always go on their own line
  (Allman). New `.c`/`.h` files use the `Includes / Macros / Typedefs / Prototypes / Global
  Variables / Exported Functions / Static Functions / EOF` section scaffold from
  `CODING_STYLE.md`.
- **Header guards** are `#ifndef __FILENAME_H_`, not `#pragma once`.
- **Comments only when the *why* is non-obvious.** `AGENTS.md` and `CODING_STYLE.md` both
  discourage unnecessary comments and doc/README creation — don't add them unless asked. (Note:
  `CODING_STYLE.md` describes a Doxygen `@brief/@param/@return` convention; follow it when a
  function genuinely needs a header comment, but don't retroactively annotate existing code.)
- **No external deps.** Only the C standard library and ESP-IDF/FreeRTOS headers.
- **Don't break existing APIs.** Legacy modules may have camelCase members or `void *config`
  signatures — preserve them; only new code follows the current rules.

## Agent skills

### Issue tracker

Issues live in GitHub Issues for this repo, managed via `gh` CLI. See `docs/agents/issue-tracker.md`.

### Triage labels

The five canonical triage roles use their default label names: `needs-triage`, `needs-info`, `ready-for-agent`, `ready-for-human`, `wontfix`. See `docs/agents/triage-labels.md`.

### Domain docs

Single-context layout — one `CONTEXT.md` + `docs/adr/` at the repo root. See `docs/agents/domain.md`.
