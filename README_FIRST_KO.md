# MrJanus89/CharybdisMK2 저장소 전용 오버레이 v3

대상 브랜치:

    Charybdis-RGB

이 패키지는 저장소에 이미 존재하는 실제 Charybdis shield와 기존 키맵을
교체하지 않습니다. 다음 항목만 추가/교체합니다.

- `.github/workflows/build.yml`
- `build.yaml`
- `config/west.yml`
- `custom_modules/threshold_module/`
- 적용 참고용 `patches/`

## 적용 전

GitHub에서 현재 브랜치가 반드시 `Charybdis-RGB`인지 확인하세요.

기존 캐시 대상 모듈을 제거합니다.

```bash
git rm -r --ignore-unmatch modules/threshold_module
rm -rf modules/threshold_module
```

## ZIP 적용

ZIP 내부 파일을 저장소 루트에 덮어씁니다.

그다음:

```bash
git add -A
git commit -m "Add threshold layer processor outside cached modules"
git push origin Charybdis-RGB
```

## 기존 키맵에 적용할 내용

`patches/charybdis.conf.add` 내용을 기존 `config/charybdis.conf`에 추가합니다.

`patches/charybdis.keymap.add.dtsi` 내용을 기존
`config/charybdis.keymap`에 병합합니다.

기존 키맵 파일 자체는 이 ZIP이 덮어쓰지 않습니다.

## 중요한 진단

이전 로그에서 `No shield named 'charybdis_left' found`가 나온 것은 빌드 당시
체크아웃된 커밋/브랜치에 `config/boards/shields/charybdis`가 없었다는 의미입니다.

새 workflow의 inspect 단계가 다음을 출력합니다.

- 실제 빌드 ref와 commit
- shield 파일 목록
- threshold module 파일 목록

inspect가 실패하면 build 단계로 넘어가지 않습니다.


## v4 수정사항

`config/west.yml`을 패키지에 직접 포함했습니다.

이 파일은 다음을 고정합니다.

- ZMK `v0.3`
- `inorichi/zmk-pmw3610-driver`의 `main`
- manifest self path `config`


## v5: 지속 이동 후 레이어 활성화

기존 `&zip_temp_layer MOUSE 5000` 대신 다음 프로세서를 사용합니다.

```dts
&zip_threshold_layer MOUSE 5000
```

설정값:

```dts
threshold = <16>;
activation-delay-ms = <300>;
movement-gap-ms = <80>;
```

동작:

1. 트랙볼 이동 시작
2. 이동 이벤트 간 공백이 80ms 이하인 상태로 300ms 유지
3. 누적 이동량이 16 이상이면 MOUSE 레이어 활성화
4. 마지막 이동으로부터 5000ms 후 레이어 해제
5. 활성화 전 이동이 80ms 이상 끊기면 300ms 측정이 처음부터 다시 시작

현재 `config/charybdis.keymap`의 이 부분:

```dts
&zip_temp_layer {
    require-prior-idle-ms = <1500>;
    ...
};

&trackball_listener {
    input-processors = <
        &zip_temp_layer MOUSE 5000
        &zip_xy_scaler 1 1
    >;
};
```

에서 `&zip_temp_layer { ... };` 블록은 제거하거나 남겨두어도 되지만,
리스너의 첫 프로세서는 반드시 `&zip_threshold_layer MOUSE 5000`으로 교체합니다.


## v6: PMW3610 `cpi` Devicetree 오류 수정

ZIP을 저장소 루트에 덮어쓴 뒤 다음 명령을 실행합니다.

```bash
python apply_pmw3610_fix.py
```

스크립트가 자동으로 수행하는 작업:

1. `config/boards/shields/charybdis/charybdis_right.overlay` 백업 생성
2. 현재 PMW3610 바인딩에서 지원하지 않는 `cpi`, `swap-xy`, `invert-x`, 입력 코드 속성 제거
3. CPI와 방향 설정을 `config/charybdis.conf`에 중복 없이 추가
4. threshold processor Kconfig 활성화

그다음:

```bash
git add -A
git commit -m "Fix PMW3610 binding and sustained mouse layer activation"
git push origin Charybdis-RGB
```


## v7 직접 덮어쓰기 수정

이번 ZIP은 아래 파일을 실제 수정본으로 포함합니다.

- `config/boards/shields/charybdis/charybdis_right.overlay`
- `patches/charybdis.conf.add` (병합 참고용)

현재 `cpi` 오류 제거에는 `apply_pmw3610_fix.py`를 별도로 실행하지 않아도 됩니다.

반드시 ZIP을 저장소 루트에 풀면서 기존 파일 덮어쓰기를 허용한 뒤,
다음 명령으로 `cpi`가 사라졌는지 확인하세요.

```bash
grep -RIn "cpi[[:space:]]*=" config/boards/shields/charybdis
```

아무 결과도 나오지 않아야 정상입니다.


## v8 Kconfig 수정

이번 ZIP은 다음 파일도 직접 교체합니다.

```text
config/boards/shields/charybdis/charybdis_right.conf
```

제거된 미지원 옵션:

```conf
CONFIG_PMW3610_INIT_POWER_UP_EXTRA_DELAY_MS=1000
CONFIG_PMW3610_REPORT_INTERVAL_MIN=8
```

ZIP을 저장소 루트에 덮어쓴 뒤 다음 명령으로 미지원 옵션이 사라졌는지 확인합니다.

```bash
grep -RInE "PMW3610_(INIT_POWER_UP_EXTRA_DELAY_MS|REPORT_INTERVAL_MIN)" config
```

아무 결과도 나오지 않아야 정상입니다.
