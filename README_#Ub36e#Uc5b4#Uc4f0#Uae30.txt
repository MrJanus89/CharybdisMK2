사용 방법
=========

1. 기존 GitHub 저장소를 ZIP으로 백업합니다.
2. 이 폴더 안의 .github, config, build.yaml을 저장소 루트에 그대로 덮어씁니다.
3. GitHub에 commit/push 합니다.
4. Actions 탭에서 빌드 결과를 확인합니다.
5. 처음 버전을 바꾸는 경우 settings_reset UF2를 양쪽에 먼저 설치한 뒤 left/right UF2를 설치합니다.

현재 동작
=========
- 오른쪽 절반이 BLE central입니다.
- 오른쪽 PMW3610 트랙볼을 zmk,input-listener로 처리합니다.
- 기본 이동 시 MOUSE 레이어가 1000ms 동안 자동 활성화됩니다.
- DIR 레이어에서 트랙볼은 스크롤로 변환됩니다.
- SNIPE 레이어에서 트랙볼 이동량은 1/3로 줄어듭니다.
- CPI는 800입니다.

조정 위치
=========
- 자동 마우스 레이어 시간: config/charybdis.keymap의 &zip_temp_layer MOUSE 1000
- 스크롤 속도: config/charybdis.keymap의 &zip_xy_scaler 1 3
- 정밀 이동 속도: config/charybdis.keymap의 &zip_xy_scaler 1 3
- 트랙볼 CPI/방향: charybdis_right.overlay의 cpi, swap-xy, invert-x

주의
====
- 이 패키지는 원본 저장소의 실제 GPIO 배선을 그대로 사용했습니다.
- GitHub Actions 실제 컴파일까지 이 환경에서 실행할 수 없어, 첫 빌드 로그에 따라 ZMK/드라이버 revision 또는 RGB SPI 설정의 추가 조정이 필요할 수 있습니다.
