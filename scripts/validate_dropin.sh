#!/usr/bin/env bash
set -euo pipefail

MODULE_DIR="custom_modules/threshold_module"
BINDING="$MODULE_DIR/dts/bindings/zmk,input-processor-threshold-layer.yaml"
SOURCE="$MODULE_DIR/src/input_processor_threshold_layer.c"

test -f build.yaml
test -f .github/workflows/build.yml
test -f "$BINDING"
test -f "$SOURCE"

COUNT="$(grep -RIl 'compatible: "zmk,input-processor-threshold-layer"' "$MODULE_DIR/dts/bindings" | wc -l)"
test "$COUNT" -eq 1

if grep -RIn "zmk/input_processor.h" "$MODULE_DIR"; then
  echo "Obsolete header found."
  exit 1
fi

echo "Drop-in structure is valid."
