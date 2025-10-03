# includeMoves 기능 구현

## 개요

KataGo 분석 엔진에 `includeMoves`와 `includeMovesMinVisits` 파라미터를 추가하여 특정 착점의 분석을 보장하는 기능입니다.

### allowMoves와의 차이점

- **allowMoves**: 허용되는 착점을 제한하지만, 분석을 보장하지는 않음
- **includeMoves**: 지정된 착점의 분석을 보장 (정책망이 낮은 확률을 주거나 isAllowedRootMove가 false여도 강제 선택)

## 파라미터

### includeMoves
- **타입**: 배열 (각 요소는 {player, moves})
- **설명**: 반드시 분석해야 하는 착점 목록
- **적용 범위**: 루트 노드에만 적용
- **예시**:
```json
"includeMoves": [
  {"player": "b", "moves": ["D16", "Q4"]},
  {"player": "w", "moves": ["D4"]}
]
```

### includeMovesMinVisits
- **타입**: 정수
- **기본값**: 1
- **범위**: 1 ~ 2^50
- **설명**: 각 includeMoves 착점의 최소 보장 방문 횟수
- **예시**: `"includeMovesMinVisits": 10`

## 방문 횟수 정책

### 분리된 카운팅
1. **includeMoves 보장 방문**: 각 includeMoves 착점이 `includeMovesMinVisits`만큼 보장받음
2. **PUCT 방문**: 정책망 기반 탐색이 최대 `maxVisits`까지 실행됨
3. **총 방문 횟수** = includeMoves 보장 방문 + PUCT 방문 (최대 maxVisits)

### 동작 예시

**예시 1**: 균형잡힌 설정
```json
{
  "maxVisits": 100,
  "includeMoves": [{"player": "b", "moves": ["D16", "Q4", "Q16"]}],
  "includeMovesMinVisits": 10
}
```
- includeMoves 보장: 3개 × 10회 = 30회
- PUCT 추가 방문: 최대 100회
- 총 방문: 최대 130회

**예시 2**: includeMoves만 실행
```json
{
  "maxVisits": 10,
  "includeMoves": [{"player": "b", "moves": ["D16", "Q4", "Q16", "D4"]}],
  "includeMovesMinVisits": 20
}
```
- includeMoves 보장: 4개 × 20회 = 80회
- PUCT 추가 방문: 최대 10회
- 총 방문: 최대 90회

## 구현 파일 및 변경사항

### 1. searchparams.h/cpp
**추가된 필드**:
```cpp
int64_t includeMovesMinVisits;
```

**초기화**:
```cpp
SearchParams::SearchParams()
  : ...
  , includeMovesMinVisits(1)
```

### 2. search.h
**추가된 멤버 변수**:
```cpp
std::vector<Loc> includeMovesBlack;
std::vector<Loc> includeMovesWhite;
```

**추가된 메서드**:
```cpp
void setIncludeMoves(const std::vector<Loc>& bVec, const std::vector<Loc>& wVec);
bool hasIncludeMovesNeedingMoreVisits() const;
int64_t getIncludeMovesGuaranteedVisits() const;
```

### 3. search.cpp
**setIncludeMoves 구현**:
```cpp
void Search::setIncludeMoves(const std::vector<Loc>& bVec, const std::vector<Loc>& wVec) {
  includeMovesBlack = bVec;
  includeMovesWhite = wVec;
}
```

**hasIncludeMovesNeedingMoreVisits 구현**:
- 각 includeMoves 착점이 `includeMovesMinVisits`에 도달했는지 확인
- 하나라도 미달이면 true 반환
- **중요**: children에 존재하지 않는 includeMoves는 건너뜀 (불법 수 처리)
- C_EMPTY 처리: 초기 보드 상태에서는 흑 includeMoves 사용

**getIncludeMovesGuaranteedVisits 구현**:
- 각 includeMoves의 min(실제 방문, minVisits) 합계 계산
- PUCT 방문과 분리하기 위한 카운팅

**종료 조건 수정** (search.cpp:542-553):
```cpp
bool shouldStop = (numPlayouts >= maxPlayouts);

// includeMoves 보장 방문과 PUCT 방문을 분리해서 카운트
if(!shouldStop) {
  int64_t guaranteedVisits = getIncludeMovesGuaranteedVisits();
  int64_t nonGuaranteedVisits = (numPlayouts + numNonPlayoutVisits) - guaranteedVisits;
  // PUCT 방문이 maxVisits에 도달하고 모든 includeMoves가 보장되었으면 종료
  if(nonGuaranteedVisits >= maxVisits && !hasIncludeMovesNeedingMoreVisits())
    shouldStop = true;
}
```

### 4. searchexplorehelpers.cpp
**includeMoves 강제 선택 로직** (루트에서만 동작):
```cpp
bool includeMovesSelected = false;
if(isRoot) {
  // C_EMPTY 처리: 초기 보드 상태(rootPla == C_EMPTY)에서는 흑으로 간주
  const std::vector<Loc>& includeMoves = (thread.pla == P_BLACK || thread.pla == C_EMPTY) ? includeMovesBlack : includeMovesWhite;
  if(includeMoves.size() > 0) {
    for(Loc moveLoc : includeMoves) {
      // 아직 방문 안함 -> 즉시 선택
      if(!alreadyTried) {
        bestChildIdx = numChildrenFound;
        bestChildMoveLoc = moveLoc;
        countEdgeVisit = true; // ⭐ 중요: includeMoves는 반드시 방문 카운트!
        includeMovesSelected = true;
        break;
      }
      // 방문 했지만 minVisits 미달 -> 선택
      else if(visits < searchParams.includeMovesMinVisits) {
        bestChildIdx = i;
        bestChildMoveLoc = moveLoc;
        countEdgeVisit = true; // ⭐ 중요: includeMoves는 반드시 방문 카운트!
        includeMovesSelected = true;
        break;
      }
    }
  }
}

// includeMoves 선택되었으면 PUCT 로직 스킵
Loc bestNewMoveLoc = Board::NULL_LOC;
float bestNewNNPolicyProb = -1.0f;
if(!includeMovesSelected) {
  // ... 정상 PUCT 로직 ...
}
```

### 5. searchresults.cpp
**includeMoves의 정렬 순서 보정**:
```cpp
// includeMoves는 정책망이 음수여도 올바른 playSelectionValue 부여
bool isIncludeMove = false;
if(rootNode == &node) {
  // C_EMPTY 처리: 초기 보드 상태에서는 흑으로 간주
  const std::vector<Loc>& includeMoves = (rootPla == P_BLACK || rootPla == C_EMPTY) ? includeMovesBlack : includeMovesWhite;
  for(Loc loc : includeMoves) {
    if(loc == moveLoc) {
      isIncludeMove = true;
      break;
    }
  }
}

if((suppressPass && moveLoc == Board::PASS_LOC) || (policyProbs[getPos(moveLoc)] < 0 && !isIncludeMove)) {
  playSelectionValues.push_back(0.0);
}
else {
  playSelectionValues.push_back((double)childWeight);
}
```

### 6. asyncbot.cpp
**AsyncBot 래퍼 추가**:
```cpp
void AsyncBot::setIncludeMoves(const std::vector<Loc>& bVec, const std::vector<Loc>& wVec) {
  stopAndWait();
  search->setIncludeMoves(bVec,wVec);
}
```

### 7. analysis.cpp
**JSON 파싱 추가**:
```cpp
// AnalyzeRequest 구조체에 추가
vector<Loc> includeMovesBlack;
vector<Loc> includeMovesWhite;

// JSON 파싱
if(input.find("includeMoves") != input.end()) {
  json& includeParamsList = input["includeMoves"];
  for(size_t i = 0; i<includeParamsList.size(); i++) {
    json& includeParams = includeParamsList[i];
    Player includePla;
    vector<Loc> parsedLocs;
    parsePlayer(includeParams, "player", includePla);
    parseBoardLocs(includeParams, "moves", parsedLocs, true);
    vector<Loc>& includeMoves = includePla == P_BLACK ? rbase.includeMovesBlack : rbase.includeMovesWhite;
    for(Loc loc: parsedLocs) {
      includeMoves.push_back(loc);
    }
  }
}

if(input.find("includeMovesMinVisits") != input.end()) {
  parseInteger(input, "includeMovesMinVisits", rbase.params.includeMovesMinVisits, 1, (int64_t)1 << 50, "Must be an integer from 1 to 2^50");
}

// AsyncBot에 설정
bot->setIncludeMoves(request->includeMovesBlack,request->includeMovesWhite);
```

## 사용 예시

### 분석 쿼리 JSON
```json
{
  "id": "example1",
  "initialStones": [],
  "moves": [["B","D4"],["W","Q16"]],
  "rules": "chinese",
  "komi": 7.5,
  "boardXSize": 19,
  "boardYSize": 19,
  "analyzeTurns": [2],
  "maxVisits": 100,
  "includeMoves": [
    {"player": "b", "moves": ["D16", "Q4", "Q16"]}
  ],
  "includeMovesMinVisits": 10
}
```

### 결과 특징
- D16, Q4, Q16은 반드시 결과에 포함됨 (각 최소 10회 방문)
- 정책망이 낮은 확률을 부여해도 분석됨
- 방문 횟수에 따라 올바르게 정렬됨
- 전체 방문 횟수는 includeMoves 보장(30회) + PUCT(최대 100회) = 최대 130회

## 주의사항

1. **루트 전용**: includeMoves는 루트 노드에만 적용되며, 트리 탐색 중에는 적용되지 않음
2. **강제 선택**: isAllowedRootMove가 false여도 강제로 선택됨 (정책망 무시)
3. **방문 독립성**: includeMoves 보장 방문과 PUCT 방문은 독립적으로 카운트됨
4. **총 방문 증가**: 총 방문 횟수는 maxVisits를 초과할 수 있음
5. **파라미터 균형**: includeMovesMinVisits × 착점 개수가 너무 크면 PUCT가 충분히 실행되지 않을 수 있음

## 주요 이슈 및 해결

### 이슈 1: rootPla == C_EMPTY 미처리
**증상**: 초기 보드 상태(빈 보드)에서 includeMoves가 무시됨

**원인**: `rootPla == P_BLACK`만 체크하여 `rootPla == C_EMPTY` 상태를 처리하지 못함

**해결**: 4개 위치에서 조건을 `(rootPla == P_BLACK || rootPla == C_EMPTY)`로 변경
- search.cpp: `hasIncludeMovesNeedingMoreVisits()`, `getIncludeMovesGuaranteedVisits()`
- searchresults.cpp: `getAnalysisJson()`의 isIncludeMove 체크
- searchexplorehelpers.cpp: includeMoves 선택 로직

### 이슈 2: countEdgeVisit 플래그 미설정으로 인한 무한 루프
**증상**: includeMoves 착점이 선택되지만 방문 횟수가 0으로 유지되어 무한 루프 발생

**원인**: humanSL 로직이 `countEdgeVisit = false`로 설정한 상태에서 includeMoves 선택 시 이를 `true`로 재설정하지 않음

**해결**: searchexplorehelpers.cpp의 includeMoves 선택 로직 2개 위치에서 `countEdgeVisit = true` 추가
- 미방문 includeMoves 선택 시
- 방문 미달 includeMoves 재선택 시

### 이슈 3: 불법 수 무한 대기
**증상**: includeMoves에 불법 수가 포함되면 분석이 무한 대기

**원인**: `hasIncludeMovesNeedingMoreVisits()`가 children에 없는 includeMoves에 대해서도 true 반환

**해결**: children에서 찾지 못한 includeMoves는 건너뛰도록 로직 수정

### 이슈 4: 이미 착수된 위치 처리
**증상**: includeMoves에 이미 착수된 위치가 포함되면 무한 루프 발생

**원인**: 이미 착수된 위치는 불법 수이지만, isLegal() 체크 없이 선택을 시도하여 무한 반복

**해결**: searchexplorehelpers.cpp의 미방문 includeMoves 선택 시 isLegal() 체크 추가
```cpp
// 미방문 includeMoves 선택 전 합법성 체크
bool isLegal = thread.history.isLegal(thread.board, moveLoc, thread.pla);
if(!isLegal) {
  // 불법 수(이미 착수된 위치 등) - 건너뜀
  continue;
}
```

## 최종 테스트 결과

### 테스트 케이스: 14개 includeMoves (2개는 이미 착수됨)
```json
{
  "moves": [["B","Q4"],["W","C16"]],
  "analyzeTurns": [2],
  "maxVisits": 100,
  "includeMoves": [{
    "player": "B",
    "moves": ["Q3","R3","D4","P4","Q4","R4","C16","D16","C17","D17","E3","P3","E4","Q16"]
  }],
  "includeMovesMinVisits": 1
}
```

**결과**:
- 예상 includeMoves: 12개 (Q4, C16은 이미 착수되어 자동 필터링)
- 실제 결과: 12개 모두 포함됨 (100% 성공)
- 총 반환 moves: 111개
- 필터링된 위치: Q4, C16 (ILLEGAL로 자동 SKIP)

### 성능 특성
- 불법 수는 isLegal() 체크로 자동 필터링되어 성능 저하 없음
- children에 없는 includeMoves는 건너뛰어 불필요한 대기 없음
- countEdgeVisit 플래그로 방문 횟수 정확히 추적

## 구현 완료 상태

### 수정된 파일 목록
1. `search/searchparams.h` - includeMovesMinVisits 파라미터 정의
2. `search/searchparams.cpp` - 초기화 및 비교 연산자
3. `search/search.h` - includeMoves 멤버 변수 및 메서드 선언
4. `search/search.cpp` - 핵심 로직 구현 (종료 조건, helper 메서드)
5. `search/searchexplorehelpers.cpp` - includeMoves 강제 선택 로직 (isLegal 체크 포함)
6. `search/searchresults.cpp` - 결과 정렬 순서 보정
7. `search/asyncbot.h` - API 인터페이스
8. `search/asyncbot.cpp` - API 래퍼 구현
9. `command/analysis.cpp` - JSON 파싱 및 expectedKeys 업데이트
10. `tests/testsearchnonn.cpp` - 단위 테스트 추가

### 테스트 커버리지
- ✅ 기본 동작: includeMoves 강제 선택
- ✅ 최소 방문 보장: includeMovesMinVisits 준수
- ✅ 초기 보드 상태: C_EMPTY 처리
- ✅ 불법 수 필터링: isLegal() 체크
- ✅ 이미 착수된 위치: 자동 SKIP
- ✅ countEdgeVisit 플래그: 방문 카운트 정확성
- ✅ 정렬 순서: 음수 policy여도 올바른 playSelectionValue
