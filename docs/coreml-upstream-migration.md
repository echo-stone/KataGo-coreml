# 공식 KataGo 기반 전환 및 동기화 정책

## 목적

이 저장소는 `lightvector/KataGo`의 공식 릴리스를 정규 업스트림으로
사용하면서 KataServer에 필요한 독자 분석 기능을 유지한다.
`ChinChangYang/KataGo`는 iOS, Xcode, Metal/CoreML 실험 변경을 확인하는
보조 업스트림으로만 사용한다.

## 저장소 역할

```text
lightvector/KataGo
  └─ 공식 릴리스와 버그 수정의 기준
          │
          ▼
echo-stone/KataGo-coreml
  └─ KataServer용 분석 기능을 유지하는 제품 포크
          ▲
          │ 검토 후 필요한 변경만 선별 적용
ChinChangYang/KataGo
  └─ iOS·Xcode·Metal/CoreML 실험 기능 참고
```

권장 원격 저장소 이름은 다음과 같다.

| 이름 | 저장소 | 역할 |
| --- | --- | --- |
| `origin` | `echo-stone/KataGo-coreml` | 제품 포크 |
| `upstream` | `lightvector/KataGo` | 정규 업스트림 |
| `coreml` | `ChinChangYang/KataGo` | 실험 기능 참고 |

## 전환 기준

- 최초 전환 기준은 공식 `v1.17.2` 태그이다.
- 기존 `metal-coreml-stable` 계보는
  `legacy/coreml-stable-before-v1.17.2` 브랜치로 보존한다.
- 구형 `coremlbackend.*`, `USE_COREML_BACKEND` 및 구형 CoreML CMake 변경은
  공식 Metal 백엔드로 가져오지 않는다.
- 독자 기능은 커밋을 일괄 체리픽하지 않고 기능 단위로 최신 코드에 맞춰
  수동 이식한다.

## 보존하는 독자 기능

1. Analysis Engine의 `includeMoves`, `includeAllMoves`,
   `includeMovesMinVisits`
2. Analysis Engine의 `action: "final_score"`
3. KataServer가 사용하는 GTP·분석 검색 설정 확장
4. 로컬 macOS Metal/CoreML 빌드 도우미와 관련 테스트·문서

## 설정 마이그레이션

공식 백엔드는 CoreML을 별도 백엔드로 노출하지 않고 Metal 백엔드 안에서
MPSGraph GPU와 CoreML/ANE를 함께 사용한다.

| 기존 키 | 공식 키 | 변환 규칙 |
| --- | --- | --- |
| `coremlDeviceToUseThread<N>` | `metalDeviceToUseThread<N>` | `0`은 GPU, `100`은 ANE |
| `coremlUseFP16` | `metalUseFP16` | ANE 사용 시 기본값 `true` 권장 |

기존 장치 값 `101`, `102`는 공식 백엔드에서 유효하지 않다. 여러 ANE
스레드를 사용하려면 각 스레드에 동일하게 `100`을 지정한다.

```text
numNNServerThreadsPerModel = 4
metalDeviceToUseThread0 = 0
metalDeviceToUseThread1 = 0
metalDeviceToUseThread2 = 100
metalDeviceToUseThread3 = 100
```

## 업스트림 동기화 절차

1. `upstream`의 새 안정 태그를 가져온다.
2. 현재 제품 브랜치에서 별도 이관 워크트리를 만든다.
3. 새 태그를 제품 브랜치에 병합한다.
4. 충돌을 기능 단위로 해결하며 구형 백엔드 코드가 되살아나지 않았는지
   확인한다.
5. C++ 단위 테스트, Analysis Engine 회귀 테스트, Metal 빌드,
   KataServer E2E를 실행한다.
6. 검증된 변경만 제품 기본 브랜치에 병합한다.

큰 버전 변경에서 백엔드 구조가 다시 바뀐 경우에는 기존 제품 브랜치에
직접 병합하지 않고 새 공식 태그에서 독자 기능을 재이식한다.

## 필수 검증

| 영역 | 검증 내용 |
| --- | --- |
| 공통 C++ | `katago runtests`와 관련 분석 테스트 |
| Analysis Engine | 포함 착점 보장, `maxVisits=0`, 취소·종료, `final_score` |
| macOS | Metal 빌드, GPU 전용, ANE 전용, GPU+ANE 혼합 |
| Linux | TensorRT 10 이상 빌드 및 KataServer 실행 |
| KataServer | 분석 API 계약과 실제 KataGo 프로세스 E2E |

모델 파일이나 하드웨어가 없어 실행할 수 없는 검증은 사용한 명령과
제약 사항을 변경 보고서에 남긴다.
