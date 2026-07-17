# Threshold layer module notes (v11)

- Uses `ZMK_EXTRA_MODULES` in `build.yaml`.
- Input processor has exactly two phandle parameters: layer and timeout-ms.
- Movement threshold is configured with the node property `threshold = <16>;`.
- The binding intentionally does **not** include `base.yaml`.
  In this Zephyr/ZMK version, `base.yaml` contributes an inherited input-processor cell name,
  which made the effective `input-processor-cells` list length 3 while `#input-processor-cells` was 2.
