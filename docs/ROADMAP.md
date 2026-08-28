# VEMCODE : Roadmap

Completed items are marked `[x]`. Active and future phases are in order of planned work.

---

### Phase 0 — Core Infrastructure ✓

- [x] Initial file structure and CMakeLists.txt
- [x] `ArduinoAPI` function pointer table — all Arduino calls go through injected struct
- [x] `ArduinoRuntime` — implements all `impl_*` functions, owns simulation state
- [x] `SketchThread` — QThread running `vb_loop()` on a background thread
- [x] First working Qt6 GUI with serial monitor output
- [x] AutoCompile pipeline — file watch → g++ invocation → DLL hot-reload
- [x] Output DLL placed in sketch subfolder (not build dir)
- [x] Initial Preprocessor — transforms Arduino source to DLL format (`vb_init`, `vb_setup`, `vb_loop`)
- [x] Circuit canvas (`CanvasWidget`) with basic component rendering
- [x] Clickable Button component on canvas
- [x] `CircuitDetector` keyword scan — detects component types from `#define` names
- [x] Delay consistency — simulated delay tracks sketch timing

> **Milestone:** "Blink" sketch compiles, loads, and toggles the LED on the canvas. ✓

---

### Phase 1 — Editor and Language Features ✓

- [x] Syntax highlighting — blue keywords, yellow functions, green comments, orange strings
- [x] Compile error highlighting — red line backgrounds via `QTextEdit::ExtraSelection`
- [x] Error line number correction — subtracts `INJECTED_HEADER_LINES` so errors point to user sketch lines
- [x] Corrected error message names (temp file path stripped, `api->` prefix stripped)
- [x] Line number gutter — `EditorWithLines` + `LineNumberArea` subclass
- [x] Auto-indent and auto-dedent — Enter carries indentation, Tab inserts 4 spaces, `}` dedents
- [x] Variable watch panel — `QTableWidget` updated from `watch_variable()` callbacks
- [x] `Serial.println` type overloads — int, float, String, const char*
- [x] `String` class — wraps `std::string`, injected into preprocessor header
- [x] Math functions — `map()`, `constrain()`, `abs()`, `min()`, `max()`, `random()`
- [x] Safety delay injection — preprocessor inserts `api->delay(10)` if no delay found in `loop()`, prevents infinite loop crash

> **Milestone:** Non-trivial sketches using String and math helpers compile and run without crashes. ✓

---

### Phase 2 — UI Polish and User Workflow ✓

- [x] First-run settings dialog — compiler path and project root saved to `app/settings.ini`
- [x] New Sketch button — creates empty sketch in a new subfolder
- [x] Recent Sketches button — last 5 paths persisted in `settings.ini`
- [x] Speed slider — range 1–25 (= 0.1x–2.5x), passed to runtime as `speed_multiplier = 1/speed`
- [x] Stop delay fix — `impl_delay` sleeps in 10ms chunks, checks `stop_requested_` between each chunk
- [x] `Serial.available()` / `Serial.read()` — UI text input feeds `serial_buffer_`, consumed by runtime
- [x] Switch component — toggles state on click, persists in `switchStates_` QMap
- [x] Potentiometer component — drag up/down changes analog value 0–1023
- [x] Two-column canvas layout — inputs left, outputs right, pin-aligned wiring
- [x] Signal timeline — logic analyzer waveform view for digital pin state history

> **Milestone:** Full interactive sketch workflow: open, edit, compile, run, adjust inputs, stop. ✓

---

### Phase 3 — Component Completion ✓

- [x] `pulseIn(pin, value, timeout)` — fast path (distance sensor), color channel path (TCS3200), slow path (pin polling)
- [x] `delayMicroseconds` — busy-wait with stop check
- [x] `analogWrite` fires `on_pin_changed` for signal timeline tracking
- [x] Array-based pin detection (`const int PIN[N] = {...}`)
- [x] Multi-pin component grouping (HC-SR04 → DistanceSensor, H-bridge → HBridgeMotor, TCS3200 → ColorSensor)
- [x] Motor (H-bridge) separated from Servo (PWM single pin)
- [x] Canvas sensor inputs — distance (cm → µs), color (R/G/B 0-255), analog (0-1023)
- [x] Servo angle display — live °label updated from analogWrite value
- [x] `Servo` class — injected inline by `strip_includes()` replacing `#include <Servo.h>`
- [x] Preprocessor `strip_includes()` step — runs before `replace_api_calls()`, handles library header replacement
- [x] Temperature, light, and generic analog sensor canvas inputs
- [x] HBridge motor PWM pin detection and speed display

> **Milestone:** Target benchmark sketch compiles and runs correctly. ✓

---

### Phase 4 — Board Profiles ✓

- [x] `BoardProfile` struct in `src/core/runtime/boardprofile.h` — `name`, `chip`, `pin_count`, `analog_offset`, `analog_count`, `pwm_resolution`, `serial_count`
- [x] Built-in profiles: Arduino Uno (ATmega328P), Arduino Nano (ATmega328P), Arduino Mega 2560 (ATmega2560), Arduino Due (AT91SAM3X8E), Teensy 4.1 (IMXRT1062)
- [x] Board selector in Settings dialog — saved to `board/name` in `settings.ini`
- [x] `RuntimeState` pin arrays bumped to fixed `[80]` / `[20]` max, all hardcoded `20`/`14`/`8` replaced with profile values
- [x] `inject_analog` / `impl_analogRead` use `profile.analog_offset` instead of hardcoded `14`
- [x] `CanvasWidget` fully profile-aware — pin loops, pin spacing, `BOARD_H`, servo angle, board name and chip label on canvas graphic
- [x] `setProfile()` chain: `SketchThread` → `SketchHost` → `ArduinoRuntime` — board change propagates to running runtime
- [x] Unlocks running the full Lambo sketch on Teensy 4.1 without pin remapping
- [x] `// @board <name>` sketch hint — `Preprocessor::extract_board_profile()` reads the comment from raw source, surfaced via `CompileResult::board_hint`, applied by MainWindow on run (canvas, label, runtime, and settings all update automatically)

---

### Phase 5 — Cross-Platform Support ✓

- [x] Linux shared library — `sketch.so` compiled and loaded via `dlopen` / `dlsym` / `dlclose`
- [x] Platform-abstracted DLL lifecycle — `#ifdef _WIN32` / `#else` guards in `SketchHost`
- [x] Temp copy strategy consistent across platforms — `.tmp.dll` (Windows), `.tmp.so` (Linux)
- [x] Linux compiler default — `/usr/bin/g++`, detected and pre-filled in settings dialog
- [x] CMakeLists.txt links `dl` on Linux, no extra libs on Windows
- [x] Build instructions for both platforms (CMake configure + build + run)

> **Milestone:** Full compile-run-stop cycle verified on both Windows (MinGW) and Linux. ✓

---

### Phase 6 — LCD Component ✓

Add a working 16x2 LCD to the canvas. Rudimentary visuals only — characters displayed in a fixed-width grid, no pixel-accurate graphics. Pretty rendering comes later in Phase 11.

**How it works:**

The preprocessor injects a replacement `LiquidCrystal` class in `strip_includes()` (same approach as `Servo.h`). The constructor stores the RS pin as the component identifier. `lcd.print()`, `lcd.clear()`, and `lcd.setCursor()` call `api->lcd_print(rs, row, text)` — a new entry in `ArduinoAPI` that fires a callback up through `ArduinoRuntime` → `SketchThread` → `CanvasWidget`, where `QGraphicsTextItem` labels on each row are updated in real time.

- [x] `LiquidCrystal` replacement class injected by `strip_includes()` — `LiquidCrystal(rs, en, d4, d5, d6, d7)`, same approach as `Servo.h`
- [x] `lcd.begin(cols, rows)` — signals LCD active via `digitalWrite(rs, HIGH)` and clears both rows
- [x] `lcd.print(const char*)` / `lcd.print(String)` / `lcd.print(int)` / `lcd.print(float)` — all overloads call `api->lcd_print`
- [x] `lcd.setCursor(col, row)` — tracks current row for subsequent `print()` calls
- [x] `lcd.clear()` — clears both rows via `lcd_print`
- [x] `lcd_print` API function — new entry at end of `ArduinoAPI` struct; `impl_lcd_print` in `ArduinoRuntime` fires `on_lcd_print` callback
- [x] Qt signal chain — `on_lcd_print` → `emit lcdPrint(pin, row, text)` on `SketchThread` → `updateLcdText()` slot on `CanvasWidget`
- [x] Canvas renders LCD as a cyan rectangle with two rows of `QGraphicsTextItem` (Courier New 7pt, 16 chars wide), keyed in `lcdRow0Labels_` / `lcdRow1Labels_` by RS pin
- [x] CircuitDetector LCD detection — RS + EN + D4–D7 define group detected in `detect_multipin()`; RS pin used as representative; other 5 pins claimed to prevent duplicate single-pin entries

> **Milestone:** A sketch using `LiquidCrystal` prints text and the canvas displays it correctly. ✓

---

### Phase 7 — Arduino API Completion: Simple Surface + Simulation Realism ✓

Fill out the remaining commonly-used Arduino API surface and add low-level simulation realism. All items are self-contained runtime or preprocessor changes with no inter-dependencies.

**Missing functions:**
- [x] `tone(pin, frequency)` / `tone(pin, frequency, duration)` / `noTone(pin)` — buzzer/piezo support; no actual audio, just tracks state for canvas display
- [x] `attachInterrupt(pin, ISR, mode)` — `RISING`, `FALLING`, `CHANGE` constants added to `vb` namespace; callback and mode stored in `RuntimeState`; `impl_attachInterrupt` registers the ISR and `impl_digitalWrite` fires it on matching pin transitions
- [x] ISR dispatch — `impl_digitalWrite` checks `RuntimeState` for any ISR registered on the target pin after updating its state; if the transition matches the registered mode (`RISING`: LOW→HIGH, `FALLING`: HIGH→LOW, `CHANGE`: either), the ISR function pointer is called directly on the sketch thread; `interrupts_enabled_` is checked first and the call is skipped if `noInterrupts()` is active; the dispatcher temporarily sets `interrupts_enabled_ = false` before calling the ISR and restores it after, matching AVR's automatic cli/sei behaviour around interrupt execution; logically correct for rotary encoders, pulse counters, and interrupt-driven sensors even without cycle-accurate AVR timing
- [x] `ISR()` vector macro transform — preprocessor scans for `ISR(X_vect) { ... }` blocks before compilation; strips the AVR-specific macro wrapper, renames the function to `__vb_isr_X_vect()`, and injects `api->register_isr("X_vect", __vb_isr_X_vect)` calls into `vb_setup()`; `register_isr` stores handlers in `RuntimeState::isr_handlers_`; `avr/interrupt.h` and `avr/io.h` stripped silently; supported vectors and their simulation triggers:
  - [x] `INT0_vect` / `INT1_vect` → pin 2 / pin 3 transition; dispatched from `impl_digitalWrite`
  - [x] `PCINT0_vect` / `PCINT1_vect` / `PCINT2_vect` → pin-change group transitions; dispatched from `impl_digitalWrite`
  - [x] `USART_RX_vect` → fires when the user sends input via the serial monitor (`inject_serial`)
  - [x] `WDT_vect` → watchdog timeout in interrupt mode (rather than triggering a reset); coexists with the `avr/wdt.h` simulation
  - [x] Unknown vectors → surfaced as a warning: *"ISR vector 'X_vect' is not simulated — the handler will never fire"* rather than a silent compile failure
- [x] `noInterrupts()` / `interrupts()` — track enabled state in `RuntimeState::interrupts_enabled_`; preprocessor replaces calls with `api->` prefixed versions
- [x] `EEPROM.read(addr)` / `EEPROM.write(addr, val)` / `EEPROM.update()` — 1024-byte `std::array<uint8_t, 1024>` in `RuntimeState`; bounds-checked (out-of-range returns `0xFF`); `update()` skips write if value unchanged; `#include <EEPROM.h>` stripped by preprocessor; no disk persistence between sessions
- [x] `Serial1` / `Serial2` runtime — additional hardware UARTs on Mega 2560, Due, Teensy 4.1; same implementation as `Serial`, separate buffers and callbacks (`on_serial1_output`, `on_serial2_output`); preprocessor maps `Serial1.*` / `Serial2.*` calls to `api->Serial1_*` / `api->Serial2_*`
- [x] `Serial1` / `Serial2` split monitor UI — when a board with `serial_count > 1` is active, the Serial monitor tab splits horizontally into labeled panes (Serial | Serial1 | Serial2); driven by `serial_count` on `BoardProfile`; `SketchThread` emits `serial1Output` / `serial2Output` signals wired to the new monitor panes; `rebuildSerialMonitors()` rebuilds the tab when the board profile changes at runtime
- [x] `Serial.printf(format, ...)` — common on ARM and ESP32 boards; injected via the preprocessor as an overload on the `Serial` object; maps to `snprintf` into a stack buffer then calls `api->serial_print`; `#include <stdio.h>` already available in the injected header

**Missing libraries (preprocessor injection, same approach as `Servo.h`):**
- [x] `SoftwareSerial` — injected class replacing `#include <SoftwareSerial.h>`; constructor stores `rxPin`/`txPin`; `begin`, `print`/`println` (4 overloads each), `write(byte)`, `write(buf, n)`, `available`, `read`, `peek`; `listen`/`isListening`/`overflow` return stubs; output routed to main serial monitor prefixed `[SW:N]` where N is the RX pin; RX buffer injectable per-pin via `ArduinoRuntime::inject_soft_serial(rxPin, data)`; `replace_token()` preprocessor helper prevents variable names ending in `Serial` (e.g. `mySerial`) from being mis-rewritten by the `Serial.*` replacement pass
- [x] Library injection files — each injected library class lives in its own `.inc` file in `src/core/build/libs/` (`servo.inc`, `liquidcrystal.inc`, `softwareserial.inc`), embedded at build time the same way `injected_header.inc` is; `strip_includes()` is a flat table of `{header_name, const char* content}` pairs and a single loop — adding a new injectable library = add one `.inc` file, embed it in CMake, add one entry to the table
- [x] `avr/wdt.h` — watchdog timer simulation; `wdt_enable(WDTO_Xs)` starts a countdown timer in `RuntimeState` (timeout values: WDTO_15MS through WDTO_8S); `wdt_reset()` resets the countdown; if the timer expires before the next `wdt_reset()` call, the simulation triggers a virtual reset (stops the sketch thread, clears runtime state, restarts from `vb_setup()`) and surfaces a canvas message *"Watchdog reset — wdt_reset() was not called in time"*; when combined with sleep modes, watchdog expiry is the wakeup condition; `wdt_disable()` cancels the timer; injected header defines `WDTO_*` constants matching real AVR values
- [x] `avr/sleep.h` — sleep mode simulation; `set_sleep_mode(mode)` stores the requested mode in `RuntimeState` (`SLEEP_MODE_IDLE`, `SLEEP_MODE_PWR_SAVE`, `SLEEP_MODE_PWR_DOWN`, etc.); `sleep_enable()` sets a flag; `sleep_cpu()` blocks the sketch thread on a condition variable — the thread suspends and the canvas shows a *"Sleeping…"* indicator; wakeup sources release the condition variable: watchdog timer expiry (any sleep mode) or ISR dispatch firing on a pin configured with `attachInterrupt` (modes that support pin-change wakeup); `sleep_disable()` clears the flag; covers the common pattern of `wdt_enable` → `sleep_cpu()` → periodic wakeup used in battery-powered data loggers and low-power sketches

**Missing sketch structure:**
- [x] Multi-file sketch support — if a sketch folder contains `.h` or additional `.cpp` files, include them in the compile pass; `strip_includes()` must pass through `#include "localfile.h"` rather than stripping it
- [x] Safety delay injection in `while` loops — `inject_while_delays()` scans every `while(...) { }` body and injects `api->delay(1)` if no delay is present; skips `do...while` tails and bodies that already have delays; tight sensor-polling loops no longer freeze the simulation thread
- [x] `F()` macro compatibility — `F("string")` is used in a large proportion of real sketches to store string literals in AVR flash; in VEMCODE on x86 there is no flash distinction, so `F(x)` should be defined as `(x)` in the injected header; without this, any sketch using `F()` fails to compile with a cryptic error
- [x] Inline AVR assembly transform — `transform_asm_blocks()` runs early in the pipeline; handles `__asm__`, `asm`, with or without `__volatile__`/`volatile`, with or without constraint strings:
  - [x] `__asm__("nop")` → stripped silently
  - [x] `__asm__("cli")` → `api->noInterrupts()`
  - [x] `__asm__("sei")` → `api->interrupts()`
  - [x] `__asm__("sleep")` → stripped with note
  - [x] `__asm__("wdr")` → stripped with note
  - [x] `__asm__("rjmp 0")` → stripped with note
  - [x] Unrecognized instruction → stripped with warning: *"Unrecognized assembly instruction 'X' removed"*
- [x] `PROGMEM` keyword compatibility — `const char text[] PROGMEM = "..."` is common in real sketches for flash storage; `PROGMEM` is an AVR-specific GCC attribute that doesn't exist on x86; define it as empty (`#define PROGMEM`) in the injected header so sketches using it compile without errors
- [x] `#ifdef ARDUINO` / `#ifndef ARDUINO` — common pattern in cross-platform sketches that lets code detect whether it's running on real hardware; VEMCODE doesn't define `ARDUINO` so the wrong branch compiles; fix is one line in the injected header: `#define ARDUINO 100` (matching the value the real Arduino IDE defines)
- [x] `pgm_read_byte` / `pgm_read_word` / `pgm_read_dword` / `pgm_read_float` — plain pointer dereferences in the injected header; `#include <avr/pgmspace.h>` stripped silently
- [x] `<util/delay.h>` — `#define F_CPU 16000000UL`, `#define _delay_ms(ms) api->delay(...)`, `#define _delay_us(us) api->delayMicroseconds(...)`; `#include <util/delay.h>` stripped silently
- [x] `analogReference(DEFAULT/INTERNAL/EXTERNAL)` — stubbed as a no-op in the injected header

**Error UX:**
- [x] Humanized compiler errors — post-process raw g++ output before display; a regex rewrite table maps common cryptic patterns to plain-English messages:
  - `'X' was not declared in this scope` → `"'X' not found — did you forget to declare it?"`
  - `no matching function for call to 'X'` → `"Wrong arguments passed to X"`
  - `expected ';' before '}'` → `"Missing semicolon, probably the line above"`
  - `expected '}' at end of input` → `"Unclosed brace — one of your { was never closed"`
  - `lvalue required as left operand of assignment` → `"Did you mean == instead of =?"`
  - `undefined reference to 'X'` → `"Function 'X' is used but never defined"`
  - `control reaches end of non-void function` → `"Function is missing a return statement"`
  - `expected unqualified-id before '{'` → `"Code found outside a function — all code must be inside setup(), loop(), or another function"`
  - `too many/few arguments to function 'X'` → `"Wrong number of arguments passed to 'X'"`
  - `stray '\' in program` → `"Invalid character in code — this sometimes happens when copy-pasting from a website; try retyping the line"`
  - `overflow in implicit constant conversion` → `"Number is too large for this variable type — try using long instead of int"`
  - `comparison between pointer and integer` → `"Can't compare strings with == — use strcmp() or the String class"`
- [x] No-components-detected hint — after `CircuitDetector::detect()` runs, if `components_` is empty (or contains only a Serial entry), the reason matters and the message should reflect it; three distinct cases:
  - Pin definitions found but names not recognized as component keywords (e.g. `#define MY_OUTPUT 5`) → *"Pin definitions found but couldn't identify component types — try descriptive names like `LED_PIN`, `SERVO_PIN`, `BUTTON_PIN`"*
  - Hardcoded pin numbers used with no defines at all (e.g. `digitalWrite(5, HIGH)`) → *"Pin numbers are hardcoded — give them names like `const int LED_PIN = 5;` so the simulator can identify them"*
  - Pin definitions exist only in an included local header (e.g. `#include "config.h"` has the `#define`s) — `CircuitDetector` currently only scans the main `.cpp`; extend it to also scan local `.h` files pulled in by the sketch, or surface: *"No components detected — if your pin definitions are in a header file, try moving them into the main sketch"*
  - No pin usage detected at all → existing generic message
- [x] Unsupported `#include` warning — after known headers are replaced, any remaining `#include <X.h>` generates a named warning in the serial monitor before compile: *"WARNING: \<Wire.h\> is not supported by VEMCODE — calls to this library will not work"*
- [x] Missing `setup()` / `loop()` — regex-checked before invoking g++; surfaces *"Sketch is missing a setup() function"* / *"…loop() function"* instead of a wall of linker errors
- [x] Pin out of range for selected board — if a `const int` or `#define` pin value exceeds the active board's pin count, warn: *"Pin 50 is not available on the Arduino Uno (max pin 13)"*
- [x] `analogWrite()` on a non-PWM pin — cross-reference `analogWrite` call sites against the board profile's PWM pin list and warn: *"Pin X does not support PWM on the selected board — analogWrite() will have no effect"*
- [x] Same pin claimed by two components — when `CircuitDetector` would silently drop a duplicate, instead surface: *"Pin X is used by both [Component A] and [Component B] — only one will be simulated"*
- [x] `// @board` hint unrecognised — if `extract_board_profile()` finds a `// @board` comment but the name doesn't match any known profile, warn: *"Unknown board 'X' in @board hint — using currently selected board instead"*
- [x] `map()` with equal min/max — static check for `map(val, x, x, ...)` or runtime divide-by-zero guard in `impl_map`; surface *"map() called with min == max — this causes a division by zero"* instead of a silent crash
- [x] Sketch thread crash wrapper — wrap the sketch execution loop in a try/catch and install a SIGFPE/SIGSEGV handler so any unhandled exception, division by zero, or out-of-bounds crash surfaces *"Sketch crashed — check for division by zero or out-of-bounds array access"* instead of a silently frozen canvas
- [x] `delay()` inside ISR callback — static check: if a `delay()` call appears inside a function registered via `attachInterrupt()`, warn *"delay() inside an interrupt handler will hang on real Arduino — interrupts are disabled during ISR execution"*
- [x] `digitalPinToInterrupt(pin)` defined in injected header as `inline int digitalPinToInterrupt(int pin) { return pin; }` so sketches using it correctly compile without error
- [x] Pin defined as an expression — `#define LED_PIN (2+1)` or `const int LED_PIN = BASE + 3;` compiles and runs fine but `CircuitDetector` cannot evaluate the expression and silently misses the component; detect when a pin define contains operators or references another variable and warn: *"Pin 'LED_PIN' is defined as an expression — the simulator could not evaluate it and the component may not appear on the canvas; use a plain number instead"*

**Simulation accuracy warnings** *(patterns that work in VEMCODE but fail on real hardware):*
- [x] Missing `volatile` on ISR-shared variables — if a variable is written inside an `attachInterrupt` callback and read in `loop()` or `setup()`, warn: *"'X' is shared with an ISR but not declared volatile — this may work in simulation but will likely fail on real hardware"*
- [x] `String +=` in a tight loop — if `String` concatenation is detected inside `loop()` with no apparent upper bound, warn: *"Repeated String concatenation in loop() causes heap fragmentation on real Arduino — consider using a char buffer instead"*
- [x] `pinMode()` never called for a `digitalWrite()` pin — if a pin appears in a `digitalWrite()` call but has no corresponding `pinMode(pin, OUTPUT)`, warn: *"Pin X is used with digitalWrite() but never set as OUTPUT via pinMode() — it will default to INPUT on real hardware"*

**Simulation realism:**
- [x] Floating pin simulation — undriven INPUT pins return random HIGH/LOW
- [x] Button bounce simulation — rapid toggles on click before settling (~10ms); `TACT`/`CLEAN`/`IDEAL` prefix gives a `ButtonClean` component with no bounce
- [x] Optional gaussian noise on analog readings (off by default, toggle in Settings dialog)

> **Milestone:** Simple sketches using timers, interrupts, EEPROM, and additional serial ports run correctly; the simulation behaves realistically on common hardware edge cases. ✓

---

### Phase 8 — Component Plugin System + Generator + New Components ✓

Pull the component plugin architecture forward so that all new components added in this phase and beyond use the new system from day one. The dev component generator is built last, on top of the stable plugin foundation.

**Implementation order:** foundation → component migration → detector/canvas refactor → new components → generator.

**Step 1 — Foundation:**
- [x] `ComponentEventType` enum in `src/core/circuit/componentitem.h` — typed events that input components emit upward: `DigitalPress` (int 0/1), `BouncePress` (int 0/1), `AnalogValue` (int 0–1023), `PulseUs` (qulonglong microseconds), `ColorRGB` (QVariantList {r, g, b, s2_pin, s3_pin}); new Phase 8 components add entries to this enum as needed
- [x] `ComponentItem` base class in `src/core/circuit/componentitem.h/.cpp` — inherits `QGraphicsObject`; pure `boundingRect()` and `paint()`; virtual `onPinChanged(int value)` (no-op default, overridden by output components); virtual `updateText(int row, const QString& text)` (no-op default, overridden by LCD); `Q_SIGNAL void inputChanged(int pin, int eventType, QVariant value)` (emitted by input components from their own mouse event overrides); stores `pin_` set in constructor
- [x] `ComponentDefinition` struct in `src/core/circuit/componentregistry.h` — holds `type_name`, `detect_single` keyword list, `detect_multi` pin-role map, `detect_pattern` source pattern list, `is_output` flag, and `create_item` factory (`std::function<ComponentItem*(int pin, QGraphicsItem*)>`); `DetectedComponent` in `circuitdetector.h` switches from `ComponentType type` (enum) to `std::string type_name`; the `ComponentType` enum is deleted entirely — `CanvasWidget` and all callers look up by `type_name` string from this point forward
- [x] `ComponentRegistry` singleton in `src/core/circuit/componentregistry.cpp` — flat `std::vector<ComponentDefinition>`; `register_component()` called from each component file's static initializer; `find_by_type()` used by `CanvasWidget`
- [x] `CMakeLists.txt` updated to glob `src/components/*.cpp` — new component files added by dropping a file, no CMake edits needed

**Step 2 — Component migration (one file per component in `src/components/`):**
- [x] `led.cpp` — output; `onPinChanged` sets active/inactive color; keywords: `LED`, `LAMP`, `DIODE`, `INDICATOR` (unambiguous output-only terms; `LIGHT` belongs to analogsensor only)
- [x] `button.cpp` — input; overrides `mousePressEvent`/`mouseReleaseEvent`, emits `BouncePress`; keywords: `BUTTON`, `BTN`, `TACT`, `PUSH`; `ButtonClean` variant (`CLEAN`, `IDEAL` prefix) emits `DigitalPress` instead
- [x] `switch.cpp` — input; click toggles latched state, emits `DigitalPress`; keywords: `SWITCH`, `TOGGLE`, `RELAY`
- [x] `buzzer.cpp` — output; `onPinChanged` shows active indicator; keywords: `BUZZER`, `PIEZO`, `SPEAKER`, `BEEPER`
- [x] `servo.cpp` — output; `onPinChanged` updates angle label; detect pattern: `.attach(`; keywords: `SERVO`
- [x] `potentiometer.cpp` — input; overrides `mouseMoveEvent` for drag, emits `AnalogValue`; keywords: `POT`, `POTENTIOMETER`, `KNOB`, `DIAL`
- [x] `analogsensor.cpp` — input; text field, emits `AnalogValue`; keywords: `LIGHT`, `LDR`, `PHOTO`, `TEMP`, `TEMPERATURE`, `NTC`, `SENSOR` (generic fallback)
- [x] `distancesensor.cpp` — input; text field (cm → µs), emits `PulseUs`; detect pattern: `pulseIn(` paired with trig/echo timing; keywords: `TRIG`, `ECHO`, `DISTANCE`, `ULTRASONIC`, `SONAR`, `HCSR`
- [x] `hbridgemotor.cpp` — output; `onPinChanged` updates speed/direction label; multi-pin role map (PWM, CWISE, ANTI_CWISE); keywords: `MOTOR`, `HBRIDGE`, `ENA`, `IN1`
- [x] `colorsensor.cpp` — input; R/G/B text fields, emits `ColorRGB`; multi-pin role map (OUT, S2, S3); detect pattern: `pulseIn(` on a pin with `S2`/`S3` siblings; keywords: `COLOR`, `TCS`, `S2`, `S3`
- [x] `lcd.cpp` — output; `onPinChanged` not used; overrides `updateText(int row, const QString& text)` to update its own row labels; `CanvasWidget::updateLcdText(int pin, int row, const QString& text)` stays as a public method but routes through `pinItems_[pin]->updateText(row, text)` instead of separate QMaps; detect pattern: `LiquidCrystal`; multi-pin role map (RS, EN, D4–D7); RS pin is representative

**Step 3 — Detector and canvas refactor:**
- [x] `CircuitDetector` refactored to loop over the registry — detection runs in three confidence tiers: (1) `detect_pattern` source patterns first, (2) `detect_multi` pin-role matching, (3) `detect_single` keyword matching as final fallback; a match at a higher tier short-circuits the lower tiers; no component-specific knowledge remains in `CircuitDetector` itself
  - [x] `detect_single` tier — `infer_type()` replaced with `ComponentRegistry::find_by_single_keyword()`, looping registered components' `detect_single` lists instead of a hardcoded keyword ladder; fixed the LightSensor/TempSensor/LED-vs-LIGHT drift bugs this exposed along the way
  - [x] `detect_multi` tier — needs a generic grouping engine covering the four correlation strategies currently hardcoded per component: suffix-correlate (HC-SR04: `TRIGPIN1`/`ECHOPIN1` share suffix `PIN1`), prefix-correlate (HBridgeMotor: `MOTOR1_PWM`/`MOTOR1_CWISE` share prefix `MOTOR1`), array-correlate (ColorSensor: five same-length arrays matched by index), singleton (LCD: assumes one instance, grabs one of each role globally)
  - [x] `detect_multi` becomes an ordered list of (role, keywords) pairs on `ComponentDefinition` instead of `std::map` — a `map` iterates alphabetically, which silently produces the wrong `pins[]` order (e.g. `ANTI_CWISE` < `CWISE` < `PWM`) once something actually reads it; touches `hbridge_motor.cpp`, `color_sensor.cpp`, `lcd.cpp`
  - [x] `detect_pattern` tier — dispatches by pattern shape: `.method(` patterns (Servo `.attach(`) match `obj.method(pin)` directly; plain `func(` patterns (`pulseIn(`) generalize the old PING wrapper-function search (find a user function containing the pattern, then resolve pins from its call sites)
  - [x] LCD's `LiquidCrystal lcd(RS, E, D4, D5, D6, D7)` ctor-arg fallback generalized into a reusable "extract N args from a `ClassName var(...)` call" extractor, driven by `detect_multi`'s role count/order — not an LCD-specific regex
- [x] `CanvasWidget::refresh()` calls `registry.find_by_name(comp.type_name).create_item(pin, nullptr)` for each detected component — `scene_->addItem(item)` and `connect(item, &ComponentItem::inputChanged, this, &CanvasWidget::onComponentInput)` is the entire per-component setup; no per-type switch blocks
- [x] `CanvasWidget::updatePin()` calls `item->onPinChanged(value)` — items update their own visuals; `pinItems_` map changes type from `QGraphicsRectItem*` to `ComponentItem*`; all per-type QMaps (`servoLabels_`, `lcdRow0Labels_`, `motorStates_`, etc.) move into the component items themselves
- [x] All mouse handling removed from `CanvasWidget::mousePressEvent/Release/MoveEvent` — input components handle their own events via `QGraphicsObject` mouse overrides; `CanvasWidget` mouse overrides deleted or reduced to scene fallthrough only
- [x] `CanvasWidget` gains one forwarding slot `onComponentInput(int pin, int eventType, QVariant)` that re-emits `inputChanged` up to `MainWindow` — the only signal `CanvasWidget` exposes for component interaction
- [x] `MainWindow` refactored to one `onComponentInput(int pin, int eventType, QVariant)` slot with a switch on `ComponentEventType` — dispatches to the correct `sketchThread_->inject_*` call; all per-component signal/slot pairs removed

**Step 4 — New simple components:**
- [x] RGB LED — three PWM pins (R, G, B); `onPinChanged` blends channel values into a colored circle; detected from `#define` pin names containing `RED`/`GREEN`/`BLUE` as a group
- [x] Rotary encoder — two digital pins (CLK/DT) plus optional button; canvas shows a turn counter; pairs naturally with `attachInterrupt`; keywords: `CLK`, `DT`, `ENCODER`, `ROTARY`
- [x] Infrared sensor - One digital pin (OUT); canvas shows a toggle switch to activate and deactivate the IR sensor; keywords: `IR_SENSOR`, `IR`, `INFRARED`, `IR_OUT`

**Step 5 — New complex components:**
- [x] Joystick — two analog axes (X/Y, 0–1023) plus a digital button; canvas shows dual sliders and a clickable button; emits `AnalogValue` per axis and `DigitalPress` for the button; keywords: `JOYSTICK`, `JOY`, `VRX`, `VRY`
- [x] Stepper motor — step count and direction tracked from STEP/DIR or IN1–IN4 pin patterns; canvas displays a position counter and rotation indicator; keywords: `STEP`, `DIR`, `STEPPER`
- [x] Keypad matrix — 4×4 or 4×3; detected from `rowPins[]`/`colPins[]` arrays (or `ROW1../COL1..` define groups) plus a `Keypad` usage guard; real injected `Keypad.h`-equivalent class does actual `pinMode`/`digitalWrite`/`digitalRead` row scanning (`CircuitDetector::detect_keypad_matrix`, `src/core/build/libs/keypad.inc`); clickable grid on canvas with the real 4x4/4x3 membrane-keypad silkscreen layout; keywords: `ROW`, `COL`, `KEYPAD`
- [x] DHT11 / DHT22 — temperature and humidity; `#include <DHT.h>` stripped and replaced with injected class (`src/core/build/libs/dht.inc`); `dht.readTemperature()`/`readHumidity()` return canvas-injected values via new `ArduinoAPI` float hooks; canvas shows a color-sensor-style box with temperature + humidity input fields (`src/components/dht.cpp`); detected via a dedicated `CircuitDetector::detect_dht` (the `DHT dht(DHTPIN, DHTTYPE)` constructor's 2nd arg is a type selector, not a pin); keywords: `DHT`, `DHTPIN`, `DHT_PIN`

**Step 6 — New display components:**
- [x] 7-segment display — single and multi-digit, segment-accurate rendering
- [x] MAX7219 LED matrix — `LedControl.h` injection (`src/core/build/libs/ledcontrol.inc`) buffers rows locally and flushes each through a new dedicated `matrix_set_row(pin, row, bits)` runtime hook (keyed by CS pin), same shape as the LCD's `lcd_print` hook; renders an 8×8 grid toggled by `setLed`/`setRow`/`setColumn`; `setIntensity`/`shutdown` stubbed; CS/CLK/DIN multi-pin role map via both `Prefix` and bare `Singleton` strategies (`src/components/max7219.cpp`); canvas renders as a 100x100 square (matching other components' long side) with a circle dot grid rather than the standard rectangle; single device only, no daisy-chain
- [x] Basic OLED — text and simple graphics (SSD1306-compatible); `Adafruit_SSD1306.h` injection (`src/core/build/libs/ssd1306.inc`), `Adafruit_GFX.h`/`Wire.h` accepted alongside it; self-contained framebuffer class (1 byte/pixel, up to 128x64) implements `drawPixel`/`drawLine`/`drawRect`/`fillRect`/`drawCircle`/`fillCircle`/`drawBitmap`/`print`/`println` directly against the buffer, then flushes the whole thing through a new `oled_display(pin, pixels, width, height)` runtime hook once per `display()`, same buffer-then-flush shape as NeoPixel's `show()`; text uses a small hand-authored 3x5 dot-matrix font (legible approximation, not a real hardware font); detected via `CircuitDetector::detect_oled` parsing `Adafruit_SSD1306 display(width, height[, &Wire, resetPin])` — since I2C has no dedicated GPIO pin (unlike every other component so far), the canvas item keys off `resetPin` when given, or a fixed sentinel pin (900, matched by both the detector and `Adafruit_SSD1306::NO_RESET_PIN_KEY`) when it's `-1`, which is the common case for breakout modules with no RST line; renders as a scaled bitmap (`src/components/oled.cpp`, ~1.5x real pixel size) rather than the fixed 100px-wide footprint every other component uses, another MAX7219-style sizing exception; `OLED`/`SCREEN`/`DISPLAY` keywords moved off `lcd.cpp`'s fallback list onto this component's, since a bare `#define OLED_PIN` sketch should now fall back to a blank OLED rather than an LCD
- [x] NeoPixel / WS2812B strip — individually addressable RGB LEDs, single-pin protocol, configurable strip length; `Adafruit_NeoPixel.h` injection (`src/core/build/libs/neopixel.inc`) buffers pixel colors locally and flushes the whole strip through a `neopixel_show(pin, rgb, count)` runtime hook once per `show()`, matching the real library's buffer-then-flush semantics rather than per-pixel MAX7219-style flushing; detected from the `Adafruit_NeoPixel strip(count, pin[, type])` constructor (`CircuitDetector::detect_neopixel`), same "read the constructor call" pattern as MAX7219/DHT; renders as a wrapping dot grid that grows downward with pixel count (`src/components/neopixel.cpp`), capped at 256 pixels; color-order/speed flags (`NEO_GRB`/`NEO_KHZ800`) accepted for source compatibility but unused

> **Milestone:** All existing components registered through the plugin system with three-tier detection; `CircuitDetector` and `CanvasWidget` contain no per-component knowledge; adding a new component is a single self-contained file; RGB LED, rotary encoder, joystick, keypad, DHT, 7-segment, OLED, and NeoPixel sketches all run on the canvas.

---

### Phase 9 — Protocol Libraries + Low-level AVR Simulation ✓

Heavier runtime work requiring more architectural changes: bus protocol simulation, virtual device responses, and direct register access.

**Protocol libraries (preprocessor injection + virtual device responses):**
- [x] `Wire.begin` / `Wire.write` / `Wire.read` — byte-level I2C simulation; no electrical bus characteristics; device responses come from the virtual I2C device panel
- [x] Virtual I2C device panel — "Devices" tab in the debug panel; table of 7-bit address → response byte sequence; when the sketch calls `Wire.requestFrom(addr, n)`, the runtime looks up the address and returns the configured bytes; entries are editable at runtime; covers the common pattern of reading a sensor register: `Wire.beginTransmission` → `Wire.write(reg)` → `Wire.endTransmission` → `Wire.requestFrom` → `Wire.read()`
- [x] `SPI.begin` / `SPI.transfer` — byte-level SPI simulation; no electrical bus characteristics; also stubs `beginTransaction`/`endTransaction`/`SPISettings` as no-ops for real-world sketch compatibility; unlike Wire there's no address field, so device responses come from a single configurable byte sequence in the "SPI" tab that `transfer()` cycles through on every call

**Low-level AVR simulation:**
- [x] AVR GPIO register simulation — `DDRB`, `PORTB`, `PINB`, `DDRC`, `PORTC`, `PINC`, `DDRD`, `PORTD`, `PIND` as overloaded-operator structs in injected header; ATmega328P (Uno/Nano) port layout only; reads/writes map to the same pin state as `digitalWrite`/`digitalRead`/`pinMode` by routing through those same `api->` calls per affected bit; bit-mask operations (`DDRB |= (1 << PB5)`, `PORTB |= (1 << PB5)`) work correctly; also supports the real AVR quirk of writing to `PINx` to toggle the corresponding `PORTx` bit
- [x] AVR hardware timer register simulation — `TCCR1A`, `TCCR1B`, `OCR1A`, `OCR1B`, `TIMSK1`, `TCNT1` etc. as overloaded-operator structs; writes to `OCR1A`/`OCR1B` update the corresponding pin's PWM duty cycle via the existing `analogWrite` path; `TIMSK1` overflow and compare-match interrupt enable bits register callbacks in `RuntimeState` that fire on the simulated timer tick; covers sketches that configure hardware PWM or use Timer1/Timer2 for precise timing without calling `analogWrite` directly
  - [x] `TIMER1_OVF_vect` / `TIMER2_OVF_vect` → timer overflow; dispatched from the simulated timer tick when overflow interrupt enable bit is set in `TIMSK1`
  - [x] `TIMER1_COMPA_vect` / `TIMER1_COMPB_vect` → timer compare-match A/B; dispatched when `TCNT1` reaches `OCR1A` / `OCR1B`

> **Milestone:** Sketches using I2C/SPI sensor libraries compile and run; direct GPIO register writes and hardware timer configuration work correctly. ✓

---

### Phase 10 — Editor + Settings + Canvas Improvements ✓

Polish the editor into a first-class coding environment, consolidate settings, add a serial plotter, and give the canvas a proper layout system.

**Editor:**
- [x] **Code completion** — Ctrl+Sift+Space shows a filtered popup of Arduino API functions plus all functions, variables, and `#define` constants declared in the current sketch
- [x] **Member-aware dot completion** — typing `.` right after a known object immediately pops up just that object's member names (no idle wait); covers the fixed globals (`Serial`/`Serial1`/`Serial2`, `Wire`, `SPI`, `EEPROM`) directly by name, plus user-declared `LiquidCrystal`/`Servo`/`SoftwareSerial` variables via a declaration scan (`SketchLinter::scanDeclaredTypes()`) that maps variable name → type before matching against a per-type member list
- [x] **Find & Replace** — Ctrl+F opens an inline find bar; Ctrl+H adds a replace field; Enter steps through matches, Escape dismisses
- [x] **Save in-place** — Ctrl+S saves silently to the current file path when a sketch is already open; only prompts for a name on first save of a new unsaved sketch
- [x] **Autosave / crash recovery** — editor content written to a `.autosave` file in the sketch folder every 30 seconds; on next open, if an `.autosave` file is newer than the `.cpp` file, offer to restore it; file is deleted on a clean save or close
- [x] **Unsaved changes indicator** — append `*` to the window title when the editor content differs from the saved file; clear it on save
- [x] **Auto-close brackets** — typing `(`, `[`, `{`, or `"` inserts the matching closer and positions the cursor inside; typing the closer when it is the next character skips over it instead of doubling
- [x] **Bracket matching** — when the cursor sits adjacent to `(`, `)`, `{`, `}`, `[`, or `]`, highlight the matching bracket
- [x] **Comment toggle** — Ctrl+/ adds `// ` to the current line or selected lines; pressing again removes it
- [x] **Font size zoom** — Ctrl+`+` / Ctrl+`-` / Ctrl+scroll adjusts the editor font size; resets to default with Ctrl+`0`
- [x] **Duplicate line** — Ctrl+D copies the current line and inserts it on the line below
- [x] **Compile warnings** — compiler warnings surfaced in the editor alongside errors; yellow line backgrounds for warning lines with corrected line numbers
- [x] **Sketch templates** — "New Sketch" dialog offers built-in starters (Blink, Button, Serial Echo) read from `app/sketches/templates/manifest.json`; selected template's content copied into the new sketch folder
- [x] **Example sketch library** — "Open Example..." dialog groups `app/sketches/examples/manifest.json` entries by component type (LED, Servo, LCD, Distance Sensor); selecting one prompts for a name and opens it as a new sketch ready to run
- [x] **In-app Arduino API reference** — right-click a known function in the editor for a popup showing its signature, parameter descriptions, and return value; covers the core global API plus library methods unique enough to identify without ambiguity (`src/ui/editor/apireference.h`)

**Serial Plotter:**
- [x] Numeric values printed via `Serial.println()` graphed over time in a scrolling plot panel (`SerialPlotter`, new "Serial plotter" tab in the debug panel); multiple named variables supported via `label:value` tokens separated by whitespace/commas, matching the Arduino IDE Serial Plotter protocol; unlabeled bare numbers default to "Value"/"Value 2"/...; auto-scaled shared Y axis, scroll/zoom via mouse wheel (Ctrl+wheel to zoom) same as the signal timeline

**Settings panel:**
- [x] **Compiler path** — auto-detect common g++ install locations on first run (MinGW on Windows, `/usr/bin/g++` on Linux); show a validation indicator (green tick / red cross) next to the path field
- [x] **Component configuration** — CTRL+click any canvas component to open a config dialog for parameters that are invisible to the sketch itself (not things like NeoPixel strip length / keypad matrix size / MAX7219 device count, which are already read from the sketch's own code): MAX7219 rotation (0/90/180/270, physical mounting orientation), 7-segment common-cathode vs. common-anode polarity (currently hardcodes common-cathode, a real common-anode sketch renders inverted), RGB LED common-cathode vs. common-anode polarity (same inversion problem, one level up from single-channel), base LED color (currently one fixed hardcoded color regardless of the real LED's actual color, purely cosmetic); values saved to the `.vblayout` file alongside position
- [x] **App theme** — "Dark theme" toggle in Settings; one `qApp`-wide stylesheet re-themes the toolbar, panels, editor (incl. syntax highlighting), tables, and signal timeline live. Canvas board/chip/pin chrome and every component's own colors are fixed in both themes on purpose (matches wires/component identity colors)
- [x] **Auto-compile on save** — toggle in settings; when enabled, saving immediately triggers a compile without a manual Run click; separate keybind for run (Ctrl+R)
- [x] **Default sketch location** — configurable root folder for new sketches; New Sketch, Open, and Save As all use it; GUI and headless CLI share the same `sketches/default_location` setting, defaulting to the old `<app>/sketches` path
- [x] **Change Keybinds** — Keybinds tab in Settings; remap Save, Save As, Run, editor/canvas zoom, Find, code completion, duplicate line, and comment toggle; conflicts blocked on save, changes apply live

**Canvas improvements:**
- [x] Canvas layout mode — "Layout" toolbar button, components become draggable
- [x] Positions saved to `sketch_name.vblayout` next to `.cpp` file; use saved positions on load, auto-generate otherwise
- [x] Canvas zoom — Alt+= / Alt+- to zoom in/out; zoom level saved per sketch in the `.vblayout` file

> **Milestone:** The editor feels complete for day-to-day sketch writing; serial plotter graphs live data; canvas layout can be saved and restored; bit-banged protocols are decoded in the signal timeline.

---

### Phase 11 — Multi-board Simulation

Run any number of sketches simultaneously in one window, each in its own tab, rather than one board per window or one board per app instance.

**One window, tab bar, thin toolbar:** a tab bar sits under the toolbar and above the three panels — `sketch1 | sketch2 | ... | sketchN`. Clicking a tab swaps the entire editor/canvas/debug/serial scene to that sketch. `+` opens any sketch file into a new tab, not just variations of the current one. Each tab keeps running in the background when not focused — switching tabs only changes what's rendered, it never pauses the non-focused sketch's thread, which is what makes "simultaneous" actually true rather than just "switchable."

- [ ] **`SketchSession`** — new class bundling everything `MainWindow` currently owns directly for one sketch (`codeEditor_`, `highlighter_`, `canvasWidget_`, `sketchThread_`/`SketchHost`/`ArduinoRuntime`, debug panels, serial monitor). `MainWindow` shrinks to: toolbar + tab bar + a `QStackedWidget` of `SketchSession`s
- [ ] Tab bar UI — `+` to open any sketch file into a new tab; closing a tab stops that session's thread (decide: does closing while running warn first, or just stop silently)
- [ ] **Toolbar reflects the focused tab's own run state, not a single global flag** — each `SketchSession` tracks its own running/stopped state; switching tabs re-syncs the Run/Stop button to whichever session is now focused, independent of what any other tab is doing (e.g. stop sketch3, switch to still-running sketch2, button must say Stop for sketch2)
- [ ] Background tabs keep their `SketchThread` running while not focused — hiding a widget in Qt doesn't pause its thread, so this should fall out of the `SketchSession` split rather than needing new scheduling logic
- [ ] **Bridge component** — a small user-placed canvas component (configured like the Wire/SPI virtual device panels, not auto-detected from source) representing a connection to another open tab; drawn with real wires into it from the relevant pins (e.g. TX/RX), labeled "Bridge to sketchN"; data actually moves via the existing `inject_serial()` mechanism (board A's `on_serial_output` also calls board B's `inject_serial()`) — no new communication path, just a canvas-visible endpoint for a link that can't be drawn as one continuous wire since the two tabs are never on-screen at the same time
- [ ] Thread-safe state injection — replace `pin_values`, `analog_values`, and `pwm_values` arrays in `RuntimeState` with `std::atomic<int>`. Note: each `SketchSession`'s `RuntimeState` is already independent (no cross-session race exists), so this isn't strictly required by multi-board support itself — it's hardening the pre-existing GUI-thread-vs-sketch-thread race that already exists in the single-sketch case today, just worth doing now since there's more thread surface area to get right
- [ ] Enables master/slave, sensor node + controller, and I2C peripheral sketches

> **Milestone:** Three sketches open in three tabs, all running simultaneously (confirmed by each tab's Run/Stop button independently reflecting its own state); two of them communicate over a bridge component and virtual Serial with both canvases updating correctly when focused.

---

### Phase 12 — Manual Add/Remove Component Dialog

Reverse of every phase before it: instead of `CircuitDetector` inferring components from sketch code, a canvas-side dialog lets you add a component from the UI and generates the sketch code for it. To avoid two representations of the circuit drifting apart, added components are round-tripped through the existing `CircuitDetector`/`CanvasWidget::refresh()` pipeline rather than placed on the canvas directly — the sketch text stays the single source of truth, same as it is today.

**Step 1 — Dialog shell + accordion rows (UI only, no codegen yet):**
- [x] "+ Add Component" button in the canvas header next to `Layout`/`Reset` (`mainwindow.cpp` ~line 617-634)
- [x] `AddComponentDialog` — modal `QDialog`, opened via `dialog.exec()`, same pattern as `SettingsDialog`
- [ ] Accordion-row list (`QScrollArea` of custom row widgets, not a `QTableWidget`) with a collapsed header per row: nickname field, component type dropdown (populated from `ComponentRegistry::all()`), pin summary, remove button
- [ ] Row expands in place to a dynamic spec section rebuilt from the selected `ComponentDefinition` — one pin+mode field for `detect_single` types, one row per `PinRole` for `detect_multi` types
- [ ] Changing the type dropdown rebuilds/clears the spec section

**Step 2 — Codegen for curated simple components (LED, button, buzzer, servo, potentiometer):**
- [ ] Per-type template emitting a `#define` line (generated name must embed the type's `detect_single` keyword, e.g. nickname "frontDoor" + LED → `FRONTDOOR_LED_PIN`, or re-detection won't recognize it) plus one `pinMode()` call
- [ ] Insertion-point logic — top-of-file (after includes/existing `#define`s) for the declaration, inside `setup()` for the `pinMode()` call
- [ ] Marker comments wrapping each injected block, keyed by nickname (e.g. `// VEMCODE-COMPONENT:<nickname>:BEGIN` / `:END`)
- [ ] On Add: inject into `codeEditor_`, then immediately call `detector_.detect(...)` + `canvasWidget_->refresh(detector_.components())` (the same pair already run at `mainwindow.cpp:1081-1082` on Run) so the canvas updates without waiting for a Run

**Step 3 — Removal:**
- [ ] Remove button locates the nickname's marker span(s) in the editor text and deletes them
- [ ] Re-run detection/refresh after removal so the component drops off the canvas

**Step 4 — Full component library:**
- [ ] Multi-pin components (RGB LED, keypad matrix, LCD) — one spec-section layout per `PinRole` group
- [ ] I2C/SPI and constructor-pattern components (MAX7219, NeoPixel, OLED, DHT) — spec section needs scalar fields (strip length, device count, display dimensions) instead of/alongside raw pin numbers, and codegen must emit the constructor-call shape each type's `detect_pattern` expects
- [ ] Collision handling — warn/block if a nickname or generated define name already exists in the sketch

> **Milestone:** A component added purely through the dialog (no hand-typed code) appears correctly wired on the canvas and runs; removing it via the dialog cleanly drops both the canvas item and its generated code.

---

### Phase 13 — ESP32 Simulation + RTOS/Multitasking

Add ESP32 as a fully simulated board profile, not just a pin-count variant — GPIO-matrix-aware detection, a FreeRTOS-shaped task API (mandatory here, unlike AVR/STM32, since even Arduino-flavored ESP32 sketches assume a scheduler already exists underneath `loop()`), and the ESP32-specific Arduino-core API surface (LEDC PWM, Preferences/NVS, SPIFFS/LittleFS) that AVR never needed.

Wireless stays mostly mocked, consistent with VEMCODE's no-real-network-code stance — WiFi and BLE/BT calls get fake success/data through the same virtual-panel pattern as Wire/SPI, no real sockets or radio involved. Two narrow, deliberate exceptions: a Bluetooth gamepad feature that only *consumes* input the host OS already trusts (never opens a port or advertises anything), and an opt-in, loopback-only `WebServer` for testing a sketch's own HTTP logic in a real browser on the same machine — never reachable off the host.

**Foundation — board profile gating:**
- [ ] Board profile's MCU field actually gates which register/library `.inc` files get compiled in, replacing today's unconditional injection of `avr_registers.inc`/`avr_timers.inc` into every sketch regardless of board
- [ ] `BOARD_ESP32` profile entry (`boardprofile.h`) — pin count, 12-bit ADC resolution (not AVR's 10-bit); no fixed PWM resolution constant the way AVR has one, since LEDC is channel-configurable
- [ ] GPIO matrix awareness — ESP32 lets most peripheral signals route to most pins at runtime instead of a fixed table; `CircuitDetector` needs to handle configurable pin-role assignment rather than assuming a fixed peripheral-to-pin map

**RTOS / task model — new subsystem, not deferrable for ESP32:**
- [ ] FreeRTOS-shaped task API shim — `xTaskCreate`, `xTaskCreatePinnedToCore`, `vTaskDelay`, semaphores/queues; needed because arduino-esp32's own `loop()` already runs as a FreeRTOS task under the hood, and many common ESP32 libraries assume the scheduler is real even in Arduino-flavored sketches
- [ ] Decide up front: single-core cooperative/preemptive scheduler that ignores `xTaskCreatePinnedToCore`'s core argument, vs. an actual two-core model — a real fidelity trade-off, not something to leave implicit
- [ ] Task Watchdog Timer (TWDT) simulation — if a task doesn't yield within its configured window, trigger the same kind of reset real hardware does (mirrors the existing `avr/wdt.h` watchdog-reset pattern from Phase 7); catches a genuinely common real ESP32 bug class rather than just being a compatibility shim

**ESP32 Arduino-core API surface — mocked, no real radio:**
- [ ] `WiFi.h` station/AP mode — fake connect success/failure, fake IP, virtual-panel pattern (a config table the sketch reads), same shape as the Wire/SPI virtual device panel
- [ ] `BLEDevice.h` / `BluetoothSerial.h` — fake pairing and GATT read/write, no real radio touched
- [ ] LEDC PWM (`ledcSetup`/`ledcAttachPin`/`ledcWrite`, or the newer `analogWrite()` wrapper) — a channel/frequency/resolution-configurable API, not a register-write shim like AVR's `OCR1A`
- [ ] `Preferences.h` (NVS key-value storage) — distinct API and semantics from the existing `EEPROM.h` flash-backed compatibility shim
- [ ] `SPIFFS.h` / `LittleFS.h` — flash filesystem calls; no AVR equivalent existed, but this is common in real ESP32 sketches serving local web dashboards/config
- [ ] `Update.h` (OTA) — mocked/no-op, logs intent only; there's no second real device on the other end of a simulated flash

**Bluetooth controller input — real hardware, no radio protocol work required:**
- [ ] Read already-paired controller HID state from the host OS (evdev/`/dev/input/js*` on Linux, XInput/RawInput on Windows)
- [ ] Bluepad32-shaped API shim (`ControllerPtr`, `axisX()`/`axisY()`, button bitmask, `onConnect`/`onDisconnect`) injected the same way `Servo.h`/`LiquidCrystal.h` are
- [ ] Detection via `detect_custom` keyed off the constructor/init call — there's no pin to wire, so pin-keyword matching doesn't apply
- [ ] New canvas element for a wireless peripheral — a connection-state/live-input panel, not a wired `ComponentItem` on the breadboard
- [ ] `.timeline` support — a new action verb (e.g. `SET_CONTROLLER`) so controller-driven sketches stay scriptable in headless CI, consistent with how every other input is already testable
- [ ] Decide single-controller vs. multi-controller support for v1 (Bluepad32 supports several at once on real hardware)

**Local-loopback WebServer — opt-in, narrow exception to the no-real-sockets stance:**
- [ ] Real TCP accept loop bound strictly to `127.0.0.1`/`::1`, verified at startup — never `0.0.0.0`
- [ ] Explicit per-run opt-in before the listener opens, not silently triggered by `WiFi.begin()`/`server.begin()`
- [ ] Ephemeral/random port by default rather than a fixed, guessable one
- [ ] Real socket reads/writes piped into the sketch's `WiFiClient`/`WebServer` API shim, same background-thread pattern as existing Serial/I2C I/O

**Deliberately out of scope for this phase:**
- Real BLE/BT peripheral mode (the PC advertising itself as a discoverable device for a phone to connect to) — same real-radio, platform-fragmented (BlueZ/WinRT/CoreBluetooth), trust-boundary problem flagged during scoping; stays mocked
- BT/BLE HID emulation (ESP32 presenting itself as a keyboard/gamepad to something else) — same bucket as above
- ESP-NOW simulated as genuine inter-process IPC between simulated boards — a real, interesting option, but only makes sense once Phase 11's multi-board simulation exists; revisit after that milestone, not before

> **Milestone:** An ESP32-profile sketch using mocked WiFi/BLE, LEDC PWM, and FreeRTOS-shaped tasks compiles and runs; a task that doesn't yield in time triggers a simulated watchdog reset; a real Bluetooth gamepad paired to the host feeds live input into a Bluepad32-style sketch on the canvas, and that input is scriptable via `.timeline`; a sketch's `WebServer` is reachable from a browser on the same machine and confirmed unreachable from anywhere else.

---

### Later

- Step-through debugger — clickable gutter breakpoints, Step/Resume buttons; `impl_vb_breakpoint` blocks the sketch thread on a condition variable (same pattern as `impl_sleep_cpu`); variable watch and canvas already show paused state for free. Hardest part: needs a real line-boundary scanner (brace/paren-depth tracking, skip string/comment contents) to inject breakpoints safely — everything else in the preprocessor today is narrow regex, not real parsing
- Installer — QtIFW with GitHub Releases as the update repository; bundle MinGW for zero-dependency install on Windows; package for common Linux distros
- macOS support
- Additional board profiles (STM32, etc.) — add one `BoardProfile` entry each
- Component visual upgrades — QPainter-drawn visuals (LED glow, buzzer pulse, motor rotation) replacing placeholder rectangles, animated off a shared canvas `QTimer` phase value; architecture is SVG-ready, swapping to `QSvgRenderer` later is a single-file `paint()` change per component. Blocked on a friend doing the actual graphics — timeline unknown
- Memory analysis — flash/SRAM usage via `arduino-cli compile --format json` (skips raw avr-gcc, which lacks the Arduino core); blocks Run over a board's limit, warns on heap usage tracked separately in VEMCODE's own runtime; memory bar in UI, `.hex` export
- Hardware Bridge — per-pin mixing of virtual and real: some pins stay canvas-driven, others wire to a real board over USB serial, same running sketch, no code changes either way (e.g. a virtual OLED/button UI paired with a real sensor under test). Native desktop app avoids the WebSerial/browser-permission friction a webUI sim would have doing the same thing
- MicroPython / CircuitPython support — Python execution path on Pico and compatible boards using the same runtime, canvas, and signal timeline
- Signal timeline protocol decoder: The signal timeline already records every `(timestamp_µs, pin, level)` transition — the same data a logic analyzer captures. Add a decoder layer that runs over this stream.
- RTC module (DS1307/DS3231) — another I2C device with no dedicated GPIO pin, same shape as the OLED; a good test of whether the synthetic-pin-key pattern (`Adafruit_SSD1306::NO_RESET_PIN_KEY`) actually generalizes to a second device or was an OLED-specific workaround
- Accelerometer/Gyroscope (MPU6050) — another I2C component, but a different interaction shape than RTC/OLED (a live 3-axis value the user manipulates, not read-only or a framebuffer)
- Document headless + `.timeline` as a CI workflow — the pieces already exist (headless CLI, `.timeline` ASSERT/action fixtures, exit-code pass/fail); this is packaging/docs work (a GitHub Actions example, maybe a minimal Docker image with the compiler toolchain) to let users run their own sketch's logic tests in CI, not new engine code
- Multi-file sketch editing — distinct from Phase 11's multi-*board* tabs: a sketch spanning `sketch.cpp` plus a companion `.h`/`.cpp` already compiles (multi-file sketch support), but the editor has no UI to view/edit those companion files; add tabs within one sketch project for its own additional files
- Canvas export (PNG/SVG) — `QGraphicsScene::render()` to a `QImage`/`QSvgGenerator` is a small, self-contained addition; useful for docs, tutorials, forum posts, and bug reports without needing an external screenshot tool
- Colorblind-accessible component/wire identification — Phase 10 deliberately fixed wire and component colors across both themes to "match wires/component identity colors," which makes color the only signal distinguishing things like RGB LED channels or multi-pin wiring today; add a pattern/label fallback so that's not a hard accessibility wall
