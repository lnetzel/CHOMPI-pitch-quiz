# CHOMPI Pitch Quiz

A two-level ear-training game for CHOMPI, using the Daisy Seed BOOT_SRAM build setup and STM32H750 memory layout.

## Playing

Before pressing **Play**, the keyboard is a six-voice instrument: notes have a gentle 20 ms attack, sustain while held, and release over 500 ms. Retriggered or stolen voices use a short 5 ms de-click transition. Free play returns after every level's result sequence. Starting or selecting a level silences free-play voices; during a level, the keyboard remains dedicated to quiz answers. Holding SHIFT silences free-play voices, and held keys must be released and pressed again to sound afterward. A seventh note steals a releasing voice first, otherwise the oldest held voice.

Press **Play** to start a ten-question level. Listen to the generated note, then press the matching keyboard key as soon as its sustain ends; the game accepts answers while the 250 ms reference release is still fading. The game boots on **Level 1**, using all 15 natural notes from C3 through C5. **Level 2** uses all 25 notes in the same range, including the black keys.

During gameplay, a short Play press does nothing. Hold Play for **two seconds** to exit to idle free play; a blue progress bar fills the 15 white-key LEDs while held. Releasing early cancels the exit without restarting the level. The Play press that starts a level must be released before a new hold can exit it. Exiting keeps the selected level, partial results, and volume; the Play LED resumes its idle blue pulse.

Press **Loop** while waiting for an answer to hear the current quiz target again without spending an attempt or changing results. Loop is ignored during prompts, answer/feedback tones, pauses, result animations, and idle free play.

For reboot, hold **SHIFT/CHOMPI** and press **Loop** to arm confirmation. Loop blinks red and the CHOMPI panel LED turns red. Release CHOMPI and press it again to confirm, or press Loop again to cancel. Other keyboard/transport actions are consumed during confirmation, but the current game timing continues. Confirmation stops audio, fades the LEDs from green to white over 400 ms, briefly shows white and then darkness, and resets through the bootloader, matching the USB-storage firmware sequence. No bootloader flashing is performed.

The next target stays within five note steps of the previous target, clipped to C3-C5 without wrapping. Steps count white-key notes in Level 1 (including adjacent E-F and B-C) and semitones in Level 2. Repeating the previous target is allowed; the first target of each level is unrestricted. This difficulty is fixed for both levels and is not user-adjustable.

After any level's final feedback, a two-second result animation plays: scores of 9/10 or less blink the green/red results together; a perfect 10/10 always shows an animated rainbow across the ten black-key result LEDs, including after Level 2. A perfect score also plays the success signal twice more during the rainbow. Held keys do not override this animation, but SHIFT still shows the level selector. Once the animation finishes, the game returns to idle polyphonic free play with results and the selected level retained. There is no automatic advancement to another level. Play from idle starts the selected level from question 1 and clears its results without changing volume; use SHIFT plus a black key to select another level.

Hold **SHIFT/CHOMPI** and press **black key 1 or 2** (from the left) to start that level immediately, including during gameplay. While SHIFT is held, available levels are dim blue and the current level is bright blue on those black-key LEDs. Black keys 3-10 are reserved for future levels, remain dark, and do nothing under SHIFT. Releasing SHIFT restores the current level's results. Keyboard presses under SHIFT do not count as answers; held keys must be released and pressed again to answer. Holding SHIFT alone does not pause sounds or level progression.

The volume encoder adjusts output level in 5% steps, starting at 25% after boot. Turning it shows a green/orange/red volume bar across the white-key LEDs for 500 ms; the volume encoder's own panel LED indicates the level. A pressed key lights its keyboard LED while held, except that SHIFT's black-key level display takes priority. The Play exit-hold bar overrides other white-key displays while active. Quiz reference and answer tones have a 250 ms release; idle free-play notes retain their 500 ms release. The Play/Pause LED gently pulses blue while idle and stays solid blue during gameplay, including the result sequence. The Loop panel LED slowly pulses yellow during a game and is dark in idle; reboot confirmation overrides it with red. The SHIFT/CHOMPI panel LED slowly pulses red, becoming solid red during reboot confirmation.

Idle free play, reference notes, and answers always use Triangle. Feedback always uses Square: success is B5 then E6 (150 ms and 300 ms, followed by a 100 ms release); failure is E6 then B5 (150 ms and 300 ms, with a short de-click tail). Feedback starts as the answer enters release, so its notes and the answer tail overlap smoothly. You have three attempts per target. After a miss, the target is replayed; after a correct answer or three misses, the game advances automatically.

When SHIFT is not held, the ten black-key LEDs show the current level's results from left to right: **green** for a correct answer, **yellow** after the first miss, **orange** after the second miss, and **red** after three misses. Unstarted questions remain dark. Holding a key does not consume additional attempts, and answer presses during sounds are ignored.

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

The game and synthesizer can be checked without libDaisy or the ARM toolchain. The tests compile DaisySP's oscillator and low-pass filter directly; compile these unchanged third-party sources separately so project code can retain strict warning checks:

```sh
g++ -std=c++14 -Icode/libs/DaisySP/Source \
    -Icode/libs/DaisySP/Source/Utility \
    -c code/libs/DaisySP/Source/Synthesis/oscillator.cpp \
    -o /tmp/quiz_oscillator.o
g++ -std=c++14 -Icode/libs/DaisySP/Source/Utility \
    -c code/libs/DaisySP/Source/Filters/tone.cpp \
    -o /tmp/quiz_tone.o
g++ -std=c++14 -Wall -Wextra -Werror \
    -isystem code/libs/DaisySP/Source \
    code/tests/pitch_quiz_test.cpp /tmp/quiz_oscillator.o /tmp/quiz_tone.o \
    -o /tmp/pitch_quiz_test
/tmp/pitch_quiz_test
```

On MSYS2, prepend `/ucrt64/bin` to `PATH` for native tests. Where available, add `-fsanitize=address,undefined` to all compilation/link steps; those runtimes are not installed on the current Windows setup.

On-device verification still requires a CHOMPI: check six-note chords, gentle attacks, repeated notes and voice stealing without clicks, release tails, Triangle idle/reference/answer audio, Square success/failure sequences and their release, fixed five-step target selection in both levels, the Loop LED's yellow game pulse and dark idle state, SHIFT's red pulse, yellow/orange/red attempt progression, blink/rainbow completion on both levels, the two extra success signals on a perfect score, return to free play, SHIFT level selection and restored results, held-key behavior, all ten LED positions, three-miss advancement, Loop replay without attempts, Play tap/hold behavior and progress bar, and reboot arming/cancel/fresh confirmation. Confirm only the volume panel LED remains lit for a control and the SHIFT panel LED pulses red outside reboot UI.

The CHOMPI name and marks are not covered by the software license. Rename your firmware and avoid implying it is an official CHOMPI Club release; see [TRADEMARKS.md](TRADEMARKS.md).