# threshold_module

Custom ZMK v0.3.x input processor for Charybdis MK2.

## Function

1. Accumulate relative X/Y trackball movement.
2. Activate the selected layer after `threshold` movement units.
3. Keep the layer active for the timeout supplied in the processor reference.
4. Reset the timeout whenever trackball movement continues.
5. Deactivate the layer when the timeout expires.

## Important

Delete the old module folder before copying this one. Do not merge it with old files.

Only one binding with this compatible may exist:

    zmk,input-processor-threshold-layer

Only one source file is included:

    src/input_processor_threshold_layer.c

## Build argument

    -DZMK_EXTRA_MODULES=/path/to/modules/threshold_module
