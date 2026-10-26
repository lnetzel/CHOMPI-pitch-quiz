# CHOMPI Pitch Quiz

A beginner ear-training game for CHOMPI, using the Daisy Seed BOOT_SRAM build setup and STM32H750 memory layout.

## Playing

Press **Play** to start (or restart) a ten-note beginner round. Listen to the generated triangle-wave note, then press the matching keyboard key after the reference finishes. Beginner targets span C3 through C5, using only the natural notes of C major.

The volume encoder adjusts output level in 5% steps, starting at 25% after boot. Turning it shows a green volume bar across the white-key LEDs for 500 ms; the volume encoder's panel LED is also lit. A pressed key lights its keyboard LED while held. Reference and answer tones have a 500 ms release. The Play LED is green during a round, and Loop restarts the round.

Each accepted key press plays your answer, followed by an ascending success tone or a descending failure tone. You have three attempts per target. After a miss, the target is replayed; after a correct answer or three misses, the game advances automatically.

The ten black-key LEDs show completed questions from left to right: **green** for a correct answer within three attempts, **red** for three misses. Unfinished questions remain dark. Results stay visible at the end; press Play for another round. Holding a key does not consume additional attempts, and presses during sounds are ignored.

Generated audio replaces the starter's input pass-through and is sent to all four codec output channels. MIDI, SD-card support, presets, and battery management are not implemented.

Hardware mapping follows the [published CHOMPI TAPE firmware](https://github.com/CHOMPI-Club/CHOMPI/tree/a73d732613da684e4de844619b690776f0f50ccf/firmware/chompi-tape/code/src): the five active-low CD4021 registers use D7/D8/D9 (latch/clock/data), with Play at input 33. The keyboard's 25 GRB LEDs use D18/TIM3_CH2; black keys are LED indices 0–9. This project uses upstream libDaisy's raw shift-register states with local debouncing, not CHOMPI's modified libDaisy driver.
The volume encoder uses D15/D17, and its panel indicator is LED 9. This project uses upstream libDaisy's raw shift-register states with local debouncing, not CHOMPI's modified libDaisy driver.

## Toolchain

Use GNU Arm Embedded Toolchain **10.3-2021.10** (GCC 10.3.1), plus `make`. This matches TAPE's known-good toolchain. Newer GCC versions can exceed the firmware's SRAM_EXEC link region. This starter does not use TEMPO's GCC 13.3 toolchain.

## Dependencies

In the new firmware repository, add the library repositories as submodules:

```powershell
git submodule add https://github.com/electro-smith/libDaisy.git code/libs/libDaisy
git submodule add https://github.com/electro-smith/DaisySP.git code/libs/DaisySP
```

Then pin both submodules to revisions that you have built and tested. The template intentionally does not vendor or modify either library.

## Build

After adding the library submodules, build both libraries and the firmware from an MSYS2 Bash shell. On this Windows setup, prepend ARM GNU 10.3 to `PATH`:

```sh
export PATH="/c/armgnu/10.3-2021.10/bin:$PATH"
make -C code/libs/libDaisy
make -C code/libs/DaisySP
make -C code/src -j4
```

The BOOT_SRAM binary is written to `code/src/build/CHOMPI_Pitch_Quiz_V1.0.bin`. For a new project, change `TARGET` in the Makefile and replace or extend `chompi_main.cpp`.

## Native gameplay tests

The game and synthesizer can be checked without the hardware libraries or ARM toolchain:

```sh
g++ -std=c++14 -Wall -Wextra -Werror -fsanitize=address,undefined \
    code/tests/pitch_quiz_test.cpp -o /tmp/pitch_quiz_test
/tmp/pitch_quiz_test
```

On-device verification still requires a CHOMPI: check audible reference/answer/feedback tones, held-key behavior, all ten LED positions, three-miss advancement, and Play restarting a round.

The CHOMPI name and marks are not covered by the software license. Rename your firmware and avoid implying it is an official CHOMPI Club release; see [TRADEMARKS.md](TRADEMARKS.md).