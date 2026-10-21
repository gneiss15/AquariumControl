<!-- CalibrateA8T.md, Version: 1.01 -->
# Calibrate a Nous A8T using a Shelly reference

[Deutsch](CalibrateA8T.DE.md) · [Script](CalibrateA8T.py)

`CalibrateA8T.py` compares a Shelly reference with a Nous A8T in two measurement runs, reversing their order between runs. It calculates corrections for the A8T voltage, current and power calibration. By default, it reports the calculated values without saving them.

## Requirements

- A Shelly and a Nous A8T running compatible TmrSw firmware, reachable from the PC. The script checks DeviceTypes `TmrSwShellyC3V1` and `TmrSwA8T`.
- A suitable stable load that both devices can switch. The default minimum accepted power is 100 W.
- Python 3 on the PC; no additional Python packages are needed.
- The A8T calibration API must be reachable when applying values.

## Procedure

1. With the mains supply disconnected, connect Shelly → Nous A8T → load.
2. Start the script, connect the Shelly to the mains supply and acknowledge the prompt. The script switches on the upstream Shelly first, waits for the downstream A8T, then switches on the A8T.
3. After the first run, disconnect the mains supply and reverse the order: Nous A8T → Shelly → load. Connect the Nous to the mains supply and acknowledge the prompt. The script switches on the upstream Nous, waits for the Shelly, then switches on the Shelly.
4. The script records the second run, saves raw samples to CSV and calculates new A8T values. Invalid sample pairs are rejected. Corrections use each run's median ratios and the geometric mean of both runs.
5. Review the calculated values. Saving requires `--apply` and the exact additional confirmation `A8T SCHREIBEN`. After saving, the script reads the A8T calibration back and checks it.

The script temporarily overrides both relays to ON, even without `--apply`. It attempts to release these overrides when the measurement sequence ends. After interruption or loss of communication, check the devices locally. Change the physical connection order only with the mains supply disconnected.

## Commands

From the repository root, using the actual device addresses, measure and calculate without saving calibration values:

```sh
python3 Aq/tools/CalibrateA8T.py --shelly http://SHELLY-IP --nous http://NOUS-IP
```

To apply values, start with `--apply`. This repeats both measurement runs and asks for confirmation before writing:

```sh
python3 Aq/tools/CalibrateA8T.py --shelly http://SHELLY-IP --nous http://NOUS-IP --apply
```

Options:

- `--seconds`: measurement duration per order; default 30 seconds, minimum 10.
- `--interval`: sampling interval; default 1 second.
- `--min-power`: minimum power for accepted sample pairs; default 100 W.
- `--reconnect-timeout`: time allowed for a device to become reachable; default 180 seconds.
- `--output`: CSV destination; default `PowerCalibration_<timestamp>.csv`. Existing files are not overwritten.
- `--apply`: save the calculated A8T values after additional confirmation.

The Shelly calibration remains unchanged; only its relay override is controlled during measurement. This is a comparison against the selected Shelly reference, so its measurement accuracy limits the comparison. Personal measurement CSV files are not part of the public repository.
