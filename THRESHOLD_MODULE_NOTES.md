# Threshold module integration

The custom threshold input processor is located at:

`modules/threshold_module`

`build.yaml` adds it using ZMK's supported local-module argument:

`-DZMK_EXTRA_MODULES=/__w/CharybdisMK2/CharybdisMK2/modules/threshold_module`

The duplicate root-level `threshold_module` directory was removed. The module's
`zephyr/module.yml` registers its CMake, Kconfig, and devicetree binding roots.
