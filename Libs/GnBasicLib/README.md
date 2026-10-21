<!-- README.md, Version: 1.00 -->
# GnBasicLib

[Deutsch](README.DE.md)

Shared basic functions for the project firmware and applications.

## GPIO functions

| Name | Purpose |
| --- | --- |
| `Set...` | Set the output latch. |
| `Clr...` | Clear the output latch. |
| `Toggle...` | Toggle the output latch. |
| `GetOut...` | Read the output latch. |
| `GetIn...` | Read the pin state. |
| `...Pin` | Specify the pin by number. |
| `...PinM` | Specify the pin by bit mask. |

Pin modes include `PinMode_Output`, `PinMode_OutputOpenDrain`, `PinMode_Input` and `PinMode_InputPullUp`.

## CPU-specific functions

`GetCycleCount` returns the CPU cycle counter.
