# GTP 독자 검색 옵션

## 목적

KataServer가 생성과 분석 검색의 루트 정책 설정을 독립적으로 바꿀 수 있도록,
기존 `KataGo-coreml`의 GTP 확장 옵션을 공식 KataGo v1.17.2 코드에 맞춰 유지한다.
구형 CoreML 백엔드나 빌드 설정에는 의존하지 않는다.

## 옵션

| 옵션 | 적용 대상 | 범위 | 기본값 |
|---|---|---:|---:|
| `genmoveWideRootNoise` | `genmove` 검색 | 0.0~5.0 | 0.04 |
| `genmoveRootPolicyTemperature` | `genmove` 검색 | 0.01~100.0 | 1.0 |
| `genmoveRootPolicyTemperatureEarly` | 초반 `genmove` 검색 | 0.01~100.0 | 1.0 |
| `analysisRootPolicyTemperature` | 분석 검색 | 0.01~100.0 | 1.0 |
| `analysisRootPolicyTemperatureEarly` | 초반 분석 검색 | 0.01~100.0 | 1.0 |

루트 정책 온도가 1보다 작으면 정책 확률이 높은 수에 더 집중하고, 1보다 크면
분포가 더 평탄해진다. 0은 내부 계산에서 나눗셈 오류를 유발할 수 있으므로
공식 v1.17.2의 일반 루트 정책 온도와 같은 최솟값 0.01을 적용한다.

## GTP 사용

```text
kata-set-param genmoveRootPolicyTemperature 0.8
kata-set-param genmoveRootPolicyTemperatureEarly 0.9
kata-set-param analysisRootPolicyTemperature 1.1
kata-set-param analysisRootPolicyTemperatureEarly 1.2
kata-set-param genmoveWideRootNoise 0.04
```

`kata-list-params`, `kata-get-param`, `kata-get-params`에서도 위 옵션을 조회할 수
있다. `kata-set-param` 또는 `kata-set-params`로 변경하면 진행 중인 GTP
프로세스의 생성·분석 검색 설정에 각각 반영된다.

## 호환성

- `rootPolicyTemperature`와 `rootPolicyTemperatureEarly`는 공식 공통 검색 옵션이다.
- 이 문서의 `genmove*`, `analysis*` 옵션은 KataServer가 두 검색 유형을 독립적으로
  제어하기 위한 확장이다.
- 온도 범위는 공식 v1.17.2와 동일한 0.01~100.0으로 제한한다.
- CoreML/Metal 장치 설정과는 무관하므로 다른 신경망 백엔드에서도 같은 GTP
  인터페이스를 사용할 수 있다.

## 회귀 테스트

빌드된 실행 파일, 모델, 설정 파일을 차례로 전달한다.

```sh
bash tests/test_gtp_custom_search_params.sh \
  ./katago \
  tests/models/g170-b6c96-s175395328-d26788732.bin.gz \
  configs/gtp_example.cfg
```

백엔드별 추가 설정이 필요하면 쉼표로 시작하는 값을 환경 변수로 전달한다.

```sh
KATAGO_GTP_TEST_OVERRIDES=",metalDeviceToUseThread0=0" \
  bash tests/test_gtp_custom_search_params.sh \
  ./katago \
  tests/models/g170-b6c96-s175395328-d26788732.bin.gz \
  configs/gtp_example.cfg
```
