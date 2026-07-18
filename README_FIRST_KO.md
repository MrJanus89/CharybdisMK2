# 적용 순서

이 ZIP은 저장소 루트에 풀도록 만든 드롭인 구조입니다.

## 1. 이전 모듈 제거

반드시 기존 캐시 대상 폴더를 Git에서도 삭제합니다.

```bash
git rm -r --ignore-unmatch modules/threshold_module
rm -rf modules/threshold_module
```

## 2. ZIP 내용 복사

ZIP 내부의 다음 항목을 저장소 루트에 복사합니다.

```text
.github/
build.yaml
custom_modules/
patches/
scripts/
```

`.github/workflows/build.yml`과 `build.yaml`은 교체 파일입니다.

## 3. 기존 설정에 두 조각 적용

`patches/charybdis.conf.add`의 한 줄을 기존 `config/charybdis.conf`에 추가합니다.

`patches/charybdis.keymap.add.dtsi`의 내용을 기존
`config/charybdis.keymap`에 반영합니다.

이미 `zip_threshold_layer` 노드와 listener 설정이 있다면 중복해서 추가하지 마세요.

## 4. 검사

```bash
bash scripts/validate_dropin.sh
find custom_modules/threshold_module -type f -print
git ls-files | grep threshold_module
```

`git ls-files` 결과에 아래 옛 경로가 없어야 합니다.

```text
modules/threshold_module/dts/bindings/input/zmk,input-processor-threshold-layer.yaml
```

## 5. 커밋

```bash
git add -A
git commit -m "Use drop-in threshold module outside cached modules directory"
git push
```

GitHub Actions에서 `inspect`가 통과한 뒤 left/right 펌웨어 빌드가 시작됩니다.
