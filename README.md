# Threshold layer v7

이번 버전은 모듈 자동 dts_root 등록에 의존하지 않고, GitHub Actions의 실제 저장소 절대 경로를 DTS_ROOT에 직접 전달합니다.

빌드 로그의 `--bindings-dirs`에 다음 경로가 나타나야 합니다.

`/__w/CharybdisMK2/CharybdisMK2/modules/threshold_module/dts/bindings`

루트에 config, modules, build.yaml을 덮어쓰세요.
