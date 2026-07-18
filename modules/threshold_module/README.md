# Threshold temporary-layer input processor

This external ZMK v0.3 module activates a temporary layer only after cumulative
relative X/Y movement reaches a configured threshold.

```dts
/ {
    /omit-if-no-ref/ zip_threshold_layer: zip_threshold_layer {
        compatible = "zmk,input-processor-threshold-layer";
        #input-processor-cells = <2>;
        movement-threshold = <16>;
        status = "okay";
    };
};

&trackball_listener {
    input-processors = <
        &zip_threshold_layer MOUSE 1200
        &zip_xy_scaler 1 1
    >;
};
```

The two phandle parameters are `layer` and `timeout-ms`. The movement threshold
is a Devicetree property so it does not consume a third runtime parameter.
