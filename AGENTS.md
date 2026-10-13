# AGENTS.md

## 판단 원칙

- 저장소, 테스트, 명령 출력에 근거해 객관적으로 판단한다.
- 수학식을 사용하면 쉬운 자연어 설명을 함께 작성한다.

## 작업 규칙

- 검색은 `rg` 또는 `rg --files`를 우선 사용한다.
- 독립 작업 단위는 서브에이전트를 활용해 검토 위험을 줄인다.
- 사용자의 기존 변경을 명시적인 요청 없이 되돌리지 않는다.
- 코드 수정은 `.worktrees/` 아래의 새 Git 워크트리에서 수행한다.
- `cpp/build-coreml/` 등 빌드 산출물은 커밋하지 않는다.
- 새 함수에는 목적, 매개변수, 반환값을 설명하는 주석을 작성한다.
- 커밋 메시지와 새 코드 주석은 한국어로 작성한다.

## 테스트

- 동작 변경에는 단위 테스트와 가능한 범위의 E2E 테스트를 추가하고 실행한다.
- 실행할 수 없는 테스트는 사유와 검증 명령을 문서에 기록한다.

## macOS Metal/CoreML 빌드

저장소 루트에서 로컬 빌드 도우미를 실행한다.

```sh
cpp/xcode/coreml-build.sh
```

주요 명령은 다음과 같다.

```sh
cpp/xcode/coreml-build.sh --clean
cpp/xcode/coreml-build.sh --jobs 8
cpp/xcode/coreml-build.sh --install
cpp/xcode/coreml-build.sh --install --install-path "$HOME/.katago/katago"
cpp/xcode/coreml-build.sh --smoke-gtp
```

기본 빌드 디렉터리는 `cpp/build-coreml`, 기본 설치 위치는
`$HOME/.katago/katago`이다. 스크립트는 공식 `METAL` 백엔드를 사용하므로
MPSGraph GPU와 CoreML/ANE 경로를 함께 포함한다.

## 문서 인덱스

- [공식 KataGo 기반 전환 및 동기화 정책](docs/coreml-upstream-migration.md)
- [공식 KataGo v1.18.2 동기화 보고](docs/upstream-v1.18.2-sync.md)
- [Analysis Engine 프로토콜](docs/Analysis_Engine.md)
- [Analysis Engine 돌 생사 문자열 계약](docs/analysis-stone-status.md) — 일반 분석·독립 생사·최종 점수 통합
- [Analysis Engine 보드·생사·계가 v2 내부 계약](docs/analysis-board-score-v2.md)
- [showboard·5코드 개발 요청 fixture](docs/fixtures/showboard-colored-alive-requests.jsonl) — 기존 에코스톤 검증 원본과 분리
- [간단한 기보를 이용한 기능 검증](docs/simple-sgf-feature-validation.md)
- [에코스톤 문제 DB 사활 판정 검증](docs/echostone-life-judgment-validation.md)
- [GTP 독자 검색 옵션](docs/GTP_Custom_Search_Params.md)
