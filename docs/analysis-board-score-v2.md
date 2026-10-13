# Analysis Engine 보드·생사·계가 v2 내부 계약

일반 분석 요청의 `includeBoard`, `includeAliveBoard`, `includeFinalScore`는 각각 독립적인 불리언 옵션이다. 생략하면 `false`이며, 세 옵션의 여덟 조합을 모두 허용한다. 검색 중 응답에는 세 결과 필드를 넣지 않고, 완료 응답의 실제 `turnNumber` 위치에만 요청된 결과를 넣는다.

| 요청 옵션 | 일반 분석 완료 응답 필드 | 계산 기준 |
| --- | --- | --- |
| `includeBoard` | `board` | 현재 턴의 실제 판 |
| `includeAliveBoard` | `aliveBoard` | 계가 옵션이 없으면 해당 분석 검색의 ownership |
| `includeFinalScore` | `finalScoreResult` | GTP와 같은 종료 판정·원래덤 계가 |

`board`와 `aliveBoard`는 `{xSize,ySize,stones,numBlackCaptures,numWhiteCaptures}` 객체다. `stones`는 판 위에서 왼쪽 위부터 오른쪽으로 읽은 행 우선 배열이며 길이는 `xSize × ySize`다. `board.stones`는 `B`, `W`, `.`, `!`를 사용한다. `aliveBoard.stones`는 살아 있는 돌 `B`/`W`, 죽은 돌 `b`/`w`, 빈칸 `.`, 빈 단순패 금지점 `!`를 사용한다. `!`는 한 판에 최대 한 칸이다. `koLoc` 필드는 반환하지 않는다.

`finalScoreResult`는 `{winner,result,margin}` 객체다. `winner`는 `B`, `W`, `0` 중 하나이고, `result`는 `B+2.5`, `W+2.5` 또는 `0` 형식이다. `margin`은 승자 점수 차이의 절댓값이며 무승부일 때 0이다. 완료된 대국의 확정 판정 규칙과 미종료 대국의 원래덤 계가 절차는 기존 `final_score` 동작을 따른다. 계가와 생사 보드를 함께 요청하면 첫 계가 원래덤의 ownership을 생사 계산에 재사용한다. `computeLeadCancellable`이 ownership을 요청받으면 첫 원래덤에 최소 100회 또는 검색 스레드 수의 20배를 자체 적용한다. 덤 조정의 후속 검색은 생사 보드 선택 여부와 무관하게 기존 최소 50회 또는 검색 스레드 수의 10배를 유지한다.

기존 `includeAlive`가 생성하는 `alive` 배열은 그대로 유지한다. `action: "final_score"`에서 `includeAliveBoard: true`를 지정하면 기존 점수 필드와 함께 `nextPlayer`와 `aliveBoard`를 추가한다. `action: "showboard"`의 `board`도 위 공통 객체를 사용하며 `boardText`는 기존 GTP 본문을 따른다.

검증 명령은 워크트리에서 다음과 같다. 모델은 로컬 테스트 모델 경로이며, 빌드 도우미에 `--install`을 전달하지 않는다.

```sh
cpp/xcode/coreml-build.sh --jobs 8
python3 cpp/tests/test_analysis_board_score_v2.py cpp/build-coreml/katago cpp/tests/data/configs/analysis_example.cfg cpp/tests/models/g170-b6c96-s175395328-d26788732.bin.gz
python3 cpp/tests/test_analysis_showboard.py cpp/build-coreml/katago cpp/tests/data/configs/analysis_example.cfg cpp/tests/models/g170-b6c96-s175395328-d26788732.bin.gz
python3 cpp/tests/test_analysis_stone_status.py cpp/build-coreml/katago cpp/tests/data/configs/analysis_example.cfg cpp/tests/models/g170-b6c96-s175395328-d26788732.bin.gz
```
