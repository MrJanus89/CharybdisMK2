# Threshold input processor notes (v12)

The binding intentionally declares one local `input-processor-cells` entry.
ZMK's common input-processor binding contributes the other entry during binding merge.
The node still uses `#input-processor-cells = <2>` and is invoked as:

```dts
&zip_threshold_layer MOUSE 1200
```

The two numeric cells continue to reach the driver callback as `param1` (layer) and
`param2` (timeout in milliseconds).
