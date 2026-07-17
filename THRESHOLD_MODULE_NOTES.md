# Threshold module notes

This revision targets the ZMK v0.3 input processor API.

Key changes:
- Uses `<drivers/input_processor.h>`.
- Uses the two input-processor parameters supported by ZMK v0.3: layer and timeout.
- Stores the movement threshold as a devicetree node property (`threshold = <16>;`).
- Removes the previously non-functional prior-idle and excluded-position properties.
