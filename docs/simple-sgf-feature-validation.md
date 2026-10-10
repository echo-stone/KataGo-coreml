# 간단한 기보를 이용한 기능 검증

## 결론

2026-10-10, `dfa99153`의 KataGo v1.18.2 통합 바이너리로 실제 엔진을
실행했다. `alive`·`seki` 배열의 크기, 좌표, 빈칸 처리와 독립 액션,
최소 착점 방문 수, 종료 후 영토 계가가 동작했다.

**현재 `seki`는 실제 빅만 구분하지 않는다.** 아래 7수 기보의 일반 흑돌도
`seki=true`였다. 기존 계가의 공배 접촉·단수 영역을 재사용한 결과로,
출력 계약에는 맞지만 전술적으로 확정된 빅을 의미하지 않는다.

## 9×9 포획 기보

[검증 기보](fixtures/simple-feature-validation.sgf)는 중국 규칙, 덤 7.5집이며,
수순은 `B D5, W E5, B E4, W A1, B F5, W A2, B E6`이다.
마지막 수가 백 E5 한 점을 잡는다. GTP `loadsgf`와 `showboard`로 실제
재생을 확인하고, 같은 수순을 Analysis Engine에 보냈다.

```text
    A B C D E F G H J
 9  . . . . . . . . .
 8  . . . . . . . . .
 7  . . . . . . . . .
 6  . . . . X . . . .
 5  . . . X . X . . .
 4  . . . . X . . . .
 3  . . . . . . . . .
 2  O . . . . . . . .
 1  O . . . . . . . .
```

`X`는 흑, `O`는 백이다. 사용 모델은 로컬
`kata1-zhizi-b40c768nbt-fdx6d-s11272M-d5935M`이다. 일반 분석은 20 visits,
독립 생사 액션은 기존 GTP 방식의 전용 검색을 사용했다.

| 확인 항목 | 관측 결과 |
| --- | --- |
| `alive`, `seki`, `ownership` 길이 | 각각 81 |
| 일반 분석 `alive=true` | D5, E4, F5, E6 |
| 일반 분석 `seki=true` | D5, E4, F5, E6 |
| 독립 `alive` / `seki` 액션 | 각각 일반 분석과 같은 좌표 |
| 백 A1·A2 | 두 배열 모두 `false` |
| 잡힌 E5와 나머지 빈칸 | 두 배열 모두 `false` |
| `seki`와 `alive` 관계 | 모든 `seki=true` 좌표에서 `alive=true` |
| `patterns` | 배열 출력, 검출 0건 |
| 포함 착점 W D4·C3, 최소 방문 수 3 | 각각 3 visits |
| `action:"final_score"` | `B+28.5`, `scoreSource:"estimated"` |

이 기보는 미종료 상태다. A1·A2의 `alive=false`와 점수는 해당 검색의
관측값이며, 제거된 돌이나 확정 종료 계가로 해석하면 안 된다. 일반 분석과
독립 액션의 동일한 좌표도 이 실행의 결과이며 항상 일치한다는 계약은 아니다.
패턴이 없는 기보이므로 패턴 종류별 검출 정확도는 이 검사로 검증하지 않았다.

흑 네 점은 공배에 접한 같은 색 계가 영역에 속한다. 현재 `seki`는 그 영역에
속하면서 `alive=true`인 돌을 포함하므로 모두 참이 된다. 이 수순에는 양쪽이
서로 잡지 못하고 살아 있는 실제 빅 형태가 없다.

## 두 집과 일본식 종료 계가

[두 집 검증 국면](fixtures/two-eyes-japanese-validation.sgf)은 실제 대국 수순이
아닌 5×3 배치 국면이다. 흑 13점을 놓고 B2·D2를 눈으로 남긴다.

```text
    A B C D E
 3  X X X X X
 2  X . X . X
 1  X X X X X
```

이 소형 국면은 저장소의 `g170-b6c96-s175395328-d26788732.bin.gz` 모델로
검사했다. 중국 규칙의 일반 분석과 독립 액션에서 흑 13점은 `alive=true`, 눈 두 칸은
`alive=false`, `seki`는 전체 `false`였다. 일본 규칙, 덤 0집에서 흑백이
여섯 번 교대로 패스해 종료한 뒤 독립 액션을 확인했다. 이때
`preventCleanupPhase=false`를 설정했다. 분석 설정의 기본
`preventCleanupPhase=true`에서는 같은 패스를 보내도 종료 단계로 넘어가지
않아 `scoreSource:"estimated"`가 나왔다.

| 확인 항목 | 종료 후 결과 |
| --- | --- |
| `alive` | 흑 13점 참, 눈 두 칸 거짓 |
| `seki` | 15칸 모두 거짓 |
| `final_score` | `B+2.0`, `scoreSource:"finished"` |

GTP에서도 이 SGF를 `loadsgf`로 읽고 `final_score`를 실행해 `B+2.0`을 확인했다.
일본식 영토 계가는 살아 있는 흑돌의 개수를 점수에 더하지 않으며, 이 국면의
흑집은 B2·D2의 두 집이다. 종료 판정에서 `alive`는 정적 계가 결과를 사용하고,
`seki`는 기존 계가 영역 검사와 그 생존 판정을 함께 사용한다.

## 재현 및 실행 기록

검증 바이너리는 `upstream-v1.18.2-20261010` 워크트리의
`cpp/build-coreml/katago`다. 메인 체크아웃의 기존 바이너리를 사용하지 않았다.
새 체크아웃에서는 먼저 `cpp/xcode/coreml-build.sh --jobs 8`로 빌드한다.
GTP에서 `loadsgf docs/fixtures/simple-feature-validation.sgf`와 `showboard`를
실행하면 위의 7수 국면과 백 포획 수 1을 확인할 수 있다.

일반 분석 요청은 다음과 같다. 같은 판의 독립 결과는 `include*`와 착점 보장
옵션을 제외하고 `action`에 `alive`, `seki`, `final_score`를 각각 지정한다.

```json
{"id":"simple-7","boardXSize":9,"boardYSize":9,"rules":"chinese","komi":7.5,"moves":[["B","D5"],["W","E5"],["B","E4"],["W","A1"],["B","F5"],["W","A2"],["B","E6"]],"analyzeTurns":[7],"maxVisits":20,"includeAlive":true,"includeSeki":true,"includeOwnership":true,"includePatterns":true,"includeMoves":[{"turnNumber":7,"player":"W","moves":["D4","C3"]}],"includeMovesMinVisits":3}
```

원시 요청·응답·실행 명령·모델 초기화 로그는 검증 워크트리의
`cpp/build-coreml/simple-sgf-validation/` 및
`cpp/build-coreml/fixture-review/`에 보관했다. 빌드 산출물과 실행 로그는
커밋하지 않으며, 이 문서와 두 SGF에 입력 및 관측 결과를 남긴다.
