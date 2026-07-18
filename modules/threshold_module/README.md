# Threshold temporary layer input processor

ZMK v0.3-compatible input processor that activates a configured layer only
after accumulated X/Y movement reaches a threshold.

The processor intentionally uses zero phandle parameters. Layer, timeout, and
threshold are Devicetree properties on the processor node:

```dts
/ {
    /omit-if-no-ref/ zip_threshold_layer: zip_threshold_layer {
        compatible = "zmk,input-processor-threshold-layer";
        #input-processor-cells = <0>;
        layer = <MOUSE>;
        timeout-ms = <1200>;
        movement-threshold = <16>;
        status = "okay";
    };
};

&trackball_listener {
    input-processors = <
        &zip_threshold_layer
        &zip_xy_scaler 1 1
    >;
};
```
