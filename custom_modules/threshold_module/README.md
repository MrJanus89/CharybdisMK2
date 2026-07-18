# Threshold module

Location:

    custom_modules/threshold_module

This directory is intentionally outside `modules/` so stale ZMK workflow cache
contents cannot restore deleted binding files into the custom module.

Processor arguments:

    &zip_threshold_layer TARGET_LAYER TIMEOUT_MS
