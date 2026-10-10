# Analysis Engine 돌 생사 문자열 계약

## 목적과 범위

현재 판의 모든 교차점을 `alive` 문자열 배열로 반환한다. 생존은 `"A"`, 빈 교차점은
`"E"`, 죽음은 `"D"`다. 흑백 돌에 동일한 코드를 사용한다. 이는 검색 또는 종료 계가의
예상 판정이며 무조건 생존의 증명을 의미하지 않는다. 기존 `patterns` v1과 별개다.

## 요청과 응답

일반 분석은 JSON 불리언 `includeAlive`를 사용한다. 기본값은 `false`이고,
`true`인 요청의 최종 응답에만 `alive`를 추가한다. 검색 중 부분 응답에는 생략한다.
독립 `action: "alive"`는 `id`, `action`, `turnNumber`, `isDuringSearch: false`,
`alive`만 반환한다. 두 경로는 동일한 문자열 직렬화를 사용한다.

배열 길이는 `boardXSize * boardYSize`다. `ownership`과 동일하게 왼쪽 위부터
오른쪽으로, 다음 행으로 내려가는 행 우선 순서다. 흑백 평가 관점에 따라 코드를
뒤집지 않으며 `analyzeTurns`의 각 턴 실제 판 상태를 반영한다.

| 코드 | 의미 |
| --- | --- |
| `"A"` | 현재 돌이 살아 있다고 판정됨 |
| `"E"` | 현재 빈 교차점; 이전에 포획된 위치도 포함 |
| `"D"` | 현재 돌이 죽었다고 판정됨 |

```json
{"id":"life-1","action":"alive","boardXSize":3,"boardYSize":2,"rules":"chinese","komi":0,"moves":[]}
```

빈 판의 `alive`는 `["E","E","E","E","E","E"]`다.

## 평가 경로

일반 미종료 분석은 원래 검색의 ownership을 재사용한다. 독립 액션은 GTP 계가와
같이 보수적 패스와 편향 제거를 적용한 전용 검색을 사용한다. 방문 수는 최소 100회와
검색 스레드 수의 20배 중 큰 값이다. 두 경로 모두 돌무리의 자기 색 ownership 평균이
0.2보다 크고 가장 불리한 돌도 -0.6보다 클 때 생존으로 판정한다. 즉 돌무리 전체가
자기 색 영역으로 예상되며 특정 돌이 상대 영역으로 강하게 넘어가지 않아야 한다.

확정 종료 국면은 GTP와 동일한 단순 정적 계가 분기를 사용한다. 일반 미종료 분석과
독립 액션은 검색 조건이 다르므로 판정이 다를 수 있다. 내부 GTP 계산은 기존 불리언
상태 배열을 계속 사용한다. `final_score`의 필드와 의미는 유지한다.

## 제거된 빅 확장과 호환성

`includeSeki`와 `action: "seki"`, 응답 `seki`는 제거했다. `includeSeki`는
값이 `false`여도 필드가 존재하면 명시적 오류를 반환한다. `warnUnusedFields=false`도
이 오류를 끄지 않는다. `action: "seki"`는 미지원 액션 오류다. 생사 배열이 필요한
호출자는 `includeAlive` 또는 `action: "alive"`와 새 문자열 코드를 사용해야 한다.
이전 불리언 배열과는 호환되지 않는다.

공식 계가 내부의 빅 관련 계산은 보존한다. GTP `final_status_list seki`는 공식
동작대로 성공하면서 빈 목록을 반환한다. 이를 실제 빅 검출로 해석하면 안 된다.

## 검증과 이력

`cpp/tests/test_analysis_stone_status.py`는 실제 엔진으로 코드 타입, 좌표, 빈칸,
기본 생략, 부분 응답 생략, 여러 턴, 흑백 관점, 기존 ownership 보존, 독립 액션,
종료 국면과 제거된 API의 오류를 검증한다.

```sh
python3 cpp/tests/test_analysis_stone_status.py \
  cpp/build-coreml/katago \
  cpp/tests/data/configs/analysis_example.cfg \
  cpp/tests/models/g170-b6c96-s175395328-d26788732.bin.gz
```

[간단한 기보 검증](simple-sgf-feature-validation.md)과
[에코스톤 문제 DB 검증](echostone-life-judgment-validation.md), 해당 fixture는
이전 불리언·빅 확장에 대한 역사 기록으로 보존한다. 현행 프로토콜은 이 문서가 기준이다.

2026-10-10 변경 검증에서는 Metal/CoreML 빌드와 `cpp` 디렉터리에서 실행한
`build-coreml/katago runtests`가 통과했다. 기존 공식 v1.18.2의
`calculateIndependentLifeArea` 및 `calculateIndependentLifeAreaHelper`와 구현을
대조해 계가 함수의 일치를 확인했다. 에코스톤 원본 fixture를 메모리에서 변환하여
`includeSeki`를 제거하고 독립 `seki` 요청을 제외한 7개 직접 정답 문제를 재검증했다.
강한 모델 `kata1-zhizi-b40c768nbt-fdx6d-s11272M-d5935M`에서 일반 분석·독립
`alive`, 흑·백 양선번의 28응답 모두 기존 흑돌 정답을 A/D로 유지했고,
배열 길이·문자열 코드·빈칸 E 계약도 모두 일치했다. 원본 fixture는 변경하지 않았다.
