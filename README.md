# Charybdis Trackball Threshold v5

이번 버전은 모듈의 C/Kconfig 등록과 Devicetree binding 검색 경로를 각각 명시합니다.

루트 `build.yaml`의 좌/우 빌드에는 다음 두 옵션이 모두 있어야 합니다.

```yaml
cmake-args: -DZEPHYR_EXTRA_MODULES=../../modules/threshold_module -DDTS_ROOT=../../modules/threshold_module
```

- `ZEPHYR_EXTRA_MODULES`: CMakeLists.txt/Kconfig/C 소스를 빌드에 추가
- `DTS_ROOT`: `dts/bindings` 아래 YAML 바인딩을 검색 경로에 추가

저장소 구조:

```text
CharybdisMK2/
├─ build.yaml
├─ config/charybdis.keymap
└─ modules/threshold_module/
   ├─ CMakeLists.txt
   ├─ Kconfig
   ├─ dts/bindings/input/zmk,input-processor-threshold-layer.yaml
   ├─ src/input_processor_threshold_layer.c
   └─ zephyr/module.yml
```

정상 적용 시 `gen_defines.py` 로그의 `--bindings-dirs` 목록에 다음 경로가 나타나야 합니다.

```text
.../modules/threshold_module/dts/bindings
```
