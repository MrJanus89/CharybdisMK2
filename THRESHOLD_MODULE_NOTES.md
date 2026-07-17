# Threshold layer module

ZMK v0.3-compatible call form:

```dts
&zip_threshold_layer MOUSE 1200
```

The movement threshold is configured on the processor node:

```dts
zip_threshold_layer: zip_threshold_layer {
    compatible = "zmk,input-processor-threshold-layer";
    #input-processor-cells = <2>;
    threshold = <16>;
    status = "okay";
};
```

The two input-processor cells are `layer` and `timeout-ms`.
