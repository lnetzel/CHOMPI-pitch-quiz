# CHOMPI Firmware Starter

A small starting point for a new CHOMPI firmware. This is not a copy of TAPE: it contains only the BOOT_SRAM build setup, the STM32H750 memory layout, and a four-channel audio pass-through smoke test.

The example initializes the Daisy Seed and CHOMPI's external SAI codec, then routes each input channel to the matching output channel. It does not include instrument behavior, controls, LEDs, MIDI, SD-card support, presets, or battery management.

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

The BOOT_SRAM binary is written to `code/src/build/CHOMPI_STARTER.bin`. For a new project, change `TARGET` in the Makefile and replace or extend `chompi_main.cpp`.

## Starting a new firmware

Use this directory as the root of a separate GitHub repository, enable **Template repository** in that repository's settings, then create new firmware repositories with **Use this template**. Keep the custom linker script unless you have revalidated the BOOT_SRAM memory layout and linker headroom for your firmware.

The CHOMPI name and marks are not covered by the software license. Rename your firmware and avoid implying it is an official CHOMPI Club release; see [TRADEMARKS.md](TRADEMARKS.md).