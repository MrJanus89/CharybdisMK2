# Charybdis trackball threshold v6

저장소 루트에 `config`, `modules`, `build.yaml`을 그대로 덮어씁니다.

필수 구조:

```
modules/threshold_module/
├─ zephyr/
│  ├─ module.yml
│  ├─ CMakeLists.txt
│  └─ Kconfig
├─ dts/
│  └─ bindings/input/zmk,input-processor-threshold-layer.yaml
└─ src/input_processor_threshold_layer.c
```

정상 빌드 명령에는 다음이 포함되어야 합니다.

```
-DZEPHYR_MODULES=../../modules/threshold_module
```

GitHub 저장소 웹 화면에서 반드시 다음 파일이 실제로 존재하는지 확인하세요.

```
modules/threshold_module/zephyr/module.yml
```

ZIP을 로컬에서 풀지 않고 GitHub에 ZIP 자체만 업로드하면 모듈 파일이 저장소에 추가되지 않습니다.
