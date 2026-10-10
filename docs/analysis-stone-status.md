# Analysis Engine 돌의 생사·빅 판정 계약

## 목적과 범위

이 확장은 판 위에 **현재 놓인 돌**의 생존 및 빅 여부를 Analysis Engine JSON에
추가한다. `alive`는 그 돌이 해당 평가 경로에서 살아 있다고 판정되었음을,
`seki`는 살아 있는 돌 중 빅으로 판정되었음을 뜻한다. 두 값은 판정 결과이며
증명 강도나 장래의 생존 가능성을 표시하지 않는다. `seki`는 항상 `alive`의
부분집합이다. 기존 패턴 검출기 `patterns` v1의 새 종류가 아니다.

## 일반 분석 요청

| 요청 필드 | 기본값 | 최종 분석 응답 |
| --- | --- | --- |
| `includeAlive` | `false` | `true`일 때만 `alive` 배열 출력 |
| `includeSeki` | `false` | `true`일 때만 `seki` 배열 출력 |

두 필드는 JSON 불리언이다. 요청하지 않은 배열은 생략한다. 검색 중 부분 응답
(`isDuringSearch: true`)에는 요청했더라도 두 배열을 모두 싣지 않는다.
요청한 최종 응답에서는 각각 길이가 `boardXSize * boardYSize`인 평탄한 불리언
배열을 반환한다. `ownership`과 동일하게 왼쪽 위부터 오른쪽으로, 다음 행으로
내려가는 행 우선 순서다. 빈 교차점은 두 배열 모두 `false`다. 흑백 돌에 같은
좌표와 참/거짓 의미를 사용하며 현재 착수자의 관점으로 값을 뒤집지 않는다.
`analyzeTurns`를 여러 개 지정하면 각 턴의 실제 판 상태를 별도로 판정한다.

일반 분석의 미종료 국면은 원래 분석 검색에서 계산한 ownership을 재사용해
생사를 판정한다. 체인 전체의 자기 색 ownership 평균이 0.2보다 커야 하고,
가장 불리한 돌의 자기 색 ownership도 -0.6보다 커야 살아 있다고 판정한다.
즉 체인 전반이 자기 색으로 예상되고 한 돌이라도 상대 색에 강하게 넘어가지
않아야 한다. 확정 종료 국면은 ownership 대신 단순 정적 계가 판정을 사용한다.

미종료 일반 분석 결과는 별도의 계가 검색을 실행한 결과나 GTP
`final_status_list`와 다를 수 있다. 이 차이는 결과 출처를 구분해 해석해야
한다. 빅 후보는 기존 계가의 단수 및 공배 접촉·같은 색 영역 규칙을 이용하고,
최종적으로 살아 있는 돌만 `seki`에 포함한다. 신뢰도 또는 추정 메타데이터는
추가하지 않는다.

예를 들어 3×2 보드의 판 상태가 윗줄 `B . W`, 아랫줄 `. B .`라면 배열의
좌표 순서는 `[왼쪽 위, 가운데 위, 오른쪽 위, 왼쪽 아래, 가운데 아래,
오른쪽 아래]`다. 빈칸 세 곳은 반드시 `false`다.

## 독립 액션

`action: "alive"`와 `action: "seki"`는 일반 분석의 검색 결과를 재사용하지
않는다. GTP 계가에 맞춘 보수적·잡음 없는 전용 검색 또는 이미 끝난 국면의
정적 계가 분기를 사용한다. 미종료 국면의 전용 검색 방문 수는 최소 100회와
검색 스레드 수의 20배 중 큰 값이다. 그 ownership에 앞의 체인 평균·최악
임계값을 적용한다. 각 액션은 요청한 배열 **하나만** 반환한다.

```json
{"id":"life-1","action":"alive","boardXSize":19,"boardYSize":19,"rules":"chinese","komi":7.5,"moves":[],"analyzeTurns":[0]}
```

응답은 `id`, `action`, `turnNumber`, `isDuringSearch: false`, 요청한 `alive`
또는 `seki` 배열을 담는다. 배열의 길이·좌표·빈칸 규칙은 일반 분석과 같다.
`seki` 액션의 참 값은 내부 생존 판정에서도 참이어야 한다. `final_score`
액션이나 점수 응답의 필드·의미는 바꾸지 않는다.

## 검증

통합 테스트 `cpp/tests/test_analysis_stone_status.py`는 실제 Analysis Engine에
요청을 보내 기본 생략, 개별·동시 요청, 직사각형 판, 여러 턴, 빈칸, 흑백 관점,
독립 액션, 잘못된 불리언 및 액션을 확인한다. 실행 예시는 다음과 같다.

```sh
python3 cpp/tests/test_analysis_stone_status.py \
  cpp/build-coreml/katago \
  cpp/tests/data/configs/analysis_example.cfg \
  cpp/tests/models/g170-b6c96-s175395328-d26788732.bin.gz
```

실제 엔진과 모델이 준비되지 않으면 이 테스트는 실행할 수 없다. 그런 경우
Python 구문 검사는 `python3 -m py_compile cpp/tests/test_analysis_stone_status.py`로
실행하고, 엔진 통합 결과는 빌드가 준비된 환경에서 확인한다.
