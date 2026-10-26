# CHOMPI Pitch Quiz

A two-level ear-training game for CHOMPI, using the Daisy Seed BOOT_SRAM build setup and STM32H750 memory layout.

## Playing

Press **Play** to start a ten-question level. Listen to the generated triangle-wave note, then press the matching keyboard key after the reference finishes. The game boots on **Level 1**, using all 15 natural notes from C3 through C5. **Level 2** uses all 25 notes in the same range, including the black keys.

After Level 1's final feedback, a two-second result animation plays, then Level 2 starts automatically regardless of score. During every inter-level hold, scores of 9/10 or less blink the green/red results together; a perfect 10/10 instead shows an animated rainbow across the ten result LEDs. Held keys do not override this animation, but SHIFT still shows the level selector. Level 2 stops after ten questions and leaves its results visible. Play or Loop restarts the current level from question 1 and clears its results without changing volume.

Hold **SHIFT/CHOMPI** and press **black key 1 or 2** (from the left) to start that level immediately, including during gameplay. While SHIFT is held, available levels are dim blue and the current level is bright blue on those black-key LEDs. Black keys 3-10 are reserved for future levels, remain dark, and do nothing under SHIFT. Releasing SHIFT restores the current level's results. Keyboard presses under SHIFT do not count as answers; held keys must be released and pressed again to answer. Holding SHIFT alone does not pause sounds or level progression.

The volume encoder adjusts output level in 5% steps, starting at 25% after boot. Turning it shows a green/orange/red volume bar across the white-key LEDs for 500 ms; the volume encoder's own panel LED indicates the level. A pressed key lights its keyboard LED while held, except that SHIFT's black-key level display takes priority. Reference and answer tones have a 500 ms release. The Play/Pause LED gently pulses blue while idle and stays solid blue during gameplay, including the inter-level result hold. The SHIFT/CHOMPI panel LED stays dark.

Each accepted key press plays your answer, followed by an ascending success tone or a descending failure tone. You have three attempts per target. After a miss, the target is replayed; after a correct answer or three misses, the game advances automatically.

When SHIFT is not held, the ten black-key LEDs show completed questions in the current level from left to right: **green** for a correct answer within three attempts, **red** for three misses. Unfinished questions remain dark. Holding a key does not consume additional attempts, and answer presses during sounds are ignored.

Generated audio replaces the starter's input pass-through and is sent to all four codec output channels. MIDI, SD-card support, presets, and battery management are not implemented.

Hardware mapping follows the [published CHOMPI TAPE firmware](https://github.com/CHOMPI-Club/CHOMPI/tree/a73d732613da684e4de844619b690776f0f50ccf/firmware/chompi-tape/code/src): the five active-low CD4021 registers use D7/D8/D9 (latch/clock/data), with Play at input 33. The keyboard's 25 GRB LEDs use D18/TIM3_CH2; black keys are LED indices 0–9. This project uses upstream libDaisy's raw shift-register states with local debouncing, not CHOMPI's modified libDaisy driver.
The volume encoder uses D15/D17 and panel LED10 / PTH 9. Play/Pause uses input 33 and LED8 / PTH 7; Loop uses input 34. Panel LEDs use RGB order on D16/TIM5_CH4, separate from the GRB keyboard LEDs. See [the hardware UI map](hardware_ui_map.md) for physical associations.

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

On-device verification still requires a CHOMPI: check audible reference/answer/feedback tones, both target ranges, automatic Level 1-to-2 progression, SHIFT level selection and restored results, held-key behavior, all ten LED positions, three-miss advancement, and Play/Loop restarting the current level. Confirm the volume LED stays on its own encoder and the SHIFT panel LED stays dark.

The CHOMPI name and marks are not covered by the software license. Rename your firmware and avoid implying it is an official CHOMPI Club release; see [TRADEMARKS.md](TRADEMARKS.md).