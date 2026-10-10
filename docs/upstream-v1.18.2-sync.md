# 공식 KataGo v1.18.2 동기화 보고

## 기준과 범위

| 항목 | 기준 |
| --- | --- |
| 제품 포크 | `echo-stone/KataGo-coreml` |
| 최초 공식 기반 전환 | `v1.17.2` — 기존 이력으로 보존 |
| 이번 동기화 | 공식 안정 릴리스 `v1.18.2` (`fd0723fd`) |
| 제외 | `upstream/master` (`d91ea855`), 미출시 변경 |

제품 브랜치에 공식 `v1.18.2`를 병합하면서 제품 고유 기능을 유지했다.
다음 동기화도 최신 안정 릴리스 태그를 기준으로 진행한다.

## 변경 및 충돌

공식 변경에는 CUDA·ROCm용 외부 소스와 빌드 설정, ONNX 백엔드 및
관련 테스트가 크게 포함된다. Metal 백엔드에는 런타임 정보·배치 정책·
유효 장치 수 함수가 추가되었으며, 기존 Metal/CoreML 설정값을 바꾸는
변경은 확인되지 않았다. 운영 ANE 모델의 호환성은 실기기 초기화로 검증한다.

병합 충돌 파일은 `cpp/CMakeLists.txt`와 `cpp/command/runtests.cpp` 두
곳이었다. CMake에서는 새 공식 백엔드 구성과 제품의 Metal 빌드 구성을
함께 유지했고, 테스트 진입점에는 공식 테스트와 독자 테스트를 모두
등록했다.

텍스트 충돌 외에도 공식 `BoardHistory` API의 인자 변화가 테스트
`cpp/tests/testpattern.cpp`, `cpp/tests/testboardarea.cpp`,
`cpp/tests/testsearchnonn.cpp`, `cpp/tests/testrules.cpp`의 총 62곳에
영향을 주었다. 첫 Metal 빌드에서 발견한 불리언 인자 호환 문제를 새
API에 맞게 수정했다.

## 유지한 제품 고유 기능

| 영역 | 기능 |
| --- | --- |
| Analysis Engine | `includeMoves`, `includeAllMoves`, `includeMovesMinVisits`, `final_score` |
| 패턴 | `includePatterns` 및 `patterns` v1 결과 |
| 돌 상태 | `includeAlive`, `includeSeki`, 독립 `alive`·`seki` 액션과 배열 |
| GTP·검색 | KataServer용 검색 설정 확장 |
| macOS | Metal/CoreML 로컬 빌드 도우미 및 관련 검증 |

`seki`는 `alive`의 부분집합이며 패턴 종류에 속하지 않는다. 상세 계약은
[돌의 생사·빅 판정 문서](analysis-stone-status.md)를 따른다.

## 검증 결과

| 영역 | 실행 및 결과 |
| --- | --- |
| Metal 빌드 | `cpp/xcode/coreml-build.sh --jobs 8`에서 테스트 API 호환 문제를 발견·수정한 뒤 `cmake --build cpp/build-coreml --target katago -j8` 성공 |
| C++ | `katago runtests`, `katago runoutputtests` 성공 |
| Analysis Engine | `tests/test_analysis_stone_status.py`, `tests/test_analysis_patterns_owner_map.py`를 실제 바이너리·모델·설정으로 실행하여 성공 |
| GTP | `bash cpp/tests/test_gtp_custom_search_params.sh` 성공 |
| GPU·ANE | tiny `b6c96` 모델로 GPU, ANE, 혼합 구성 각각 `boardsize 9`·착수·`genmove`·종료 성공; 로그에서 MPSGraph와 CoreML 경로 초기화 확인 |
| KataServer | `go test ./katago/... ./server` 성공 |
| 추가 프로토콜 | 빌드 디렉터리의 임시 검증 스크립트 `cpp/build-coreml/analysis_upstream_regression.py` 성공: `final_score` 추정·종료·여러 턴·잘못된 착수, `maxVisits=0` 오류, `includeMoves` 3착점 각 3방문, `includeAllMoves` 5×5 전체 각 2방문, 진행 중 `terminate` 확인·최종 부분 응답 |

Linux TensorRT 및 worker1 고정 실행기 E2E는 로컬 macOS 환경에서
실행하지 않았다. 별도 Linux 환경에서 TensorRT와 CUDA를 준비한 뒤
다음 명령으로 검증해야 한다. worker1 E2E에는 고정 실행기·모델·설정이
필요하다.

```sh
# KataGo-coreml 저장소 루트
cmake -S cpp -B cpp/build-tensorrt -DUSE_BACKEND=TENSORRT -DBUILD_DISTRIBUTED=0
cmake --build cpp/build-tensorrt --target katago -j8

# kataserver 저장소 루트
go test -tags worker1_e2e ./server -run '^TestWorker1PatternRuntimeE2E$' -count=1
```
