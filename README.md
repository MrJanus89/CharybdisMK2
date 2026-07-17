# Charybdis trackball threshold v4

This version removes `#include <input/threshold_layer.dtsi>` and declares the
`zip_threshold_layer` devicetree node directly in `config/charybdis.keymap`.
This avoids the DTS include-path failure shown by GitHub Actions.

Copy these folders/files into the repository root:

- `config/charybdis.keymap`
- `modules/threshold_module/`
- `build.yaml`

The build command must still contain:

`-DZEPHYR_MODULES=../../modules/threshold_module`

Configured processor:

`&zip_threshold_layer MOUSE 1200 16`

- layer: MOUSE
- timeout: 1200 ms
- movement threshold: 16 counts
