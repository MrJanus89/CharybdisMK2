# Charybdis trackball threshold v3

이 ZIP은 저장소 루트에 그대로 덮어쓰기 위한 구조입니다.

필수 최종 구조:

- build.yaml
- config/charybdis.keymap
- modules/threshold_module/zephyr/module.yml
- modules/threshold_module/dts/input/threshold_layer.dtsi
- modules/threshold_module/dts/bindings/input/zmk,input-processor-threshold-layer.yaml
- modules/threshold_module/src/input_processor_threshold_layer.c

중요: 기존 build.yaml을 반드시 이 ZIP의 build.yaml로 교체하세요.
GitHub Actions의 West Build 로그에 다음이 실제로 표시되어야 합니다.

-DZEPHYR_MODULES=../../modules/threshold_module

표시되지 않으면 루트 build.yaml이 교체되지 않은 것입니다.
