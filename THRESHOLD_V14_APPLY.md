# Threshold v14

This archive is flat: extract directly into the repository root.

Binding fix:
- node uses `#input-processor-cells = <2>`
- custom binding lists only the additional `timeout-ms` cell because the ZMK input-processor base binding supplies the first parameter cell.
- usage remains `&zip_threshold_layer MOUSE 1200`
