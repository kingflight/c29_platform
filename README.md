# F29H85x Makefile Example

Minimal standalone Makefile project for TI F29H85x SDK.

## Location

- Project: `/home/ouweikang/Code/f29h85x_makefile_example`
- Makefile: `/home/ouweikang/Code/f29h85x_makefile_example/build/Makefile`
- SDK expected at: `/home/ouweikang/ti/f29h85x-sdk`

## Build

```bash
cd /home/ouweikang/Code/f29h85x_makefile_example/build
make
```

Optional overrides:

```bash
make CONFIG=FLASH
make rom
make ram
make BOARD=SOM
make PROFILE=debug_O0
make SRC_DIRS="src app drivers"
make C29SDK_ROOT=/custom/path/to/f29h85x-sdk
```

## Output

- ELF: `build/out/<BOARD>/<CONFIG>/f29h85x_makefile_example.out`
- BIN: `build/out/<BOARD>/<CONFIG>/f29h85x_makefile_example.bin`
- ROM image (FLASH builds): `build/out/<BOARD>/FLASH/f29h85x_makefile_example_cert.bin`

## Notes

- This example does not require SysConfig.
- Application sources live in root-level directories (default: `src/`).
- `SRC_DIRS` controls which root-level directories are scanned recursively for `.c` files.
- It uses `device.c` from `examples/device_support/source` and `driverlib.lib` from SDK libraries.
- The compiler/toolchain path comes from `imports.mak` (default expects CCS + C29 CGT under `~/ti`).
- `make rom` performs a FLASH (`--rom_model`) build and generates a signed ROM-loadable image via `mcu_rom_image_gen.py`.
