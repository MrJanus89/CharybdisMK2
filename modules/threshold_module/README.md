# Threshold layer input processor (ZMK v0.3)

The input processor specifier uses three cells in ZMK v0.3:

1. `layer` -> callback `param1`
2. `timeout-ms` -> callback `param2`
3. `track-remainders` -> consumed by the input processor pipeline

Usage:

```dts
&zip_threshold_layer MOUSE 1200 0
```

The movement threshold is currently compiled into the driver as:

```c
#define THRESHOLD_MOVEMENT_UNITS 16U
```
