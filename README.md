# Charybdis 트랙볼 임계값 모듈 v2

## 이번 수정 내용

기존 ZIP은 모듈 폴더가 존재해도 GitHub Actions의 Zephyr 빌드에 등록되지 않았고,
DTSI도 `include/input` 아래에 있어 `<input/threshold_layer.dtsi>`를 찾지 못했습니다.

v2에서는 다음을 수정했습니다.

- 모듈 위치: `modules/threshold_module`
- DTSI 위치: `modules/threshold_module/dts/input/threshold_layer.dtsi`
- `zephyr/module.yml`에 모듈 이름과 `dts_root` 등록
- 빌드 시 `-DZEPHYR_MODULES=../../modules/threshold_module` 전달

## 적용 방법

ZIP 내부의 폴더를 저장소 루트에 그대로 덮어씁니다.

최종 구조:

```text
CharybdisMK2/
├─ build.yaml
├─ config/
│  └─ charybdis.keymap
└─ modules/
   └─ threshold_module/
      ├─ CMakeLists.txt
      ├─ Kconfig
      ├─ zephyr/module.yml
      ├─ dts/input/threshold_layer.dtsi
      ├─ dts/bindings/input/zmk,input-processor-threshold-layer.yaml
      └─ src/input_processor_threshold_layer.c
```

## build.yaml 수정

기존 `charybdis_left`, `charybdis_right` 항목에 다음 줄을 추가합니다.

```yaml
cmake-args: -DZEPHYR_MODULES=../../modules/threshold_module
```

예시는 `build.yaml.example`을 참고하세요.

`settings_reset`에는 모듈 인자가 필요하지 않습니다.

## 키맵 설정

```dts
#include <input/threshold_layer.dtsi>

&zip_threshold_layer {
    require-prior-idle-ms = <500>;
    excluded-positions = <26 27 28 48 49>;
};

&trackball_listener {
    input-processors = <
        &zip_threshold_layer MOUSE 1200 16
        &zip_xy_scaler 1 1
    >;
};
```

`16`은 이동 누적 임계값이고 `1200`은 레이어 유지 시간(ms)입니다.

## 주의

이번 버전은 사용자가 올린 로그의 `No such file or directory` 오류를 해결하도록 모듈 검색 및 DTS 경로를 수정한 버전입니다.
다음 빌드에서 C API 관련 오류가 발생하면 해당 로그를 기준으로 사용 중인 ZMK v0.3 포크의 실제 input processor API에 맞춰 소스를 조정해야 합니다.
