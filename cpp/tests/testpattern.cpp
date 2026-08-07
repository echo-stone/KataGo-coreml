#include "../tests/tests.h"
#include "../pattern/detector.h"
#include "../pattern/eyefacts.h"
#include "../core/rand.h"

#include <algorithm>
#include <chrono>
#include <set>

using namespace std;
using json = nlohmann::json;

// findKind는 검출 결과에서 지정 Kind의 첫 패턴을 찾는다.
// patterns는 검색할 목록이고 kind는 대상 Kind enum이며, 반환값은 없으면 nullptr이다.
static const GoPattern::Pattern* findKind(const vector<GoPattern::Pattern>& patterns, GoPattern::Kind kind) {
  for(const GoPattern::Pattern& pattern: patterns) {
    if(pattern.kind == kind)
      return &pattern;
  }
  return nullptr;
}

// chainHasPattern은 지정 chain을 subject로 하는 BensonAlive 결과가 있는지 검사한다.
// patterns는 검출 결과, board는 좌표계, loc은 대상 chain의 한 돌이며, 반환값은 패턴 존재 여부다.
static bool chainHasPattern(const vector<GoPattern::Pattern>& patterns, const Board& board, Loc loc) {
  const Player color = board.colors[loc];
  for(const GoPattern::Pattern& pattern: patterns) {
    if(pattern.kind != GoPattern::Kind::BENSON_ALIVE || pattern.subject.color != color)
      continue;
    for(Loc point: pattern.subject.points) {
      if(board.chain_head[point] == board.chain_head[loc])
        return true;
    }
  }
  return false;
}

// checkBensonAgainstKataGoArea는 독립 certificate 결과를 KataGo pass-alive 돌 판정과 전수 대조한다.
// board는 비교할 합법 상태이고 suicideLegal은 pass-alive 자살 규칙이며, 반환값은 없다.
static void checkBensonAgainstKataGoArea(const Board& board, bool suicideLegal) {
  Rules rules = Rules::getTrompTaylorish();
  rules.multiStoneSuicideLegal = suicideLegal;
  BoardHistory hist(board,P_BLACK,rules,0,false);
  GoPattern::DetectionResult result = GoPattern::detect(board,hist);

  Color area[Board::MAX_ARR_SIZE];
  board.calculateArea(area,false,false,false,hist.suicideLegalForPassAlive());
  for(int y = 0; y < board.y_size; y++) {
    for(int x = 0; x < board.x_size; x++) {
      Loc loc = Location::getLoc(x,y,board.x_size);
      if(board.colors[loc] != P_BLACK && board.colors[loc] != P_WHITE)
        continue;
      const bool expected = area[loc] == board.colors[loc];
      testAssert(chainHasPattern(result.patterns,board,loc) == expected);
    }
  }
}

// runSmallBoardExhaustiveBensonTest는 2×2와 3×3의 모든 무활로 chain 없는 상태를 대조한다.
// 각 상태는 자살 금지·허용 pass-alive 규칙으로 각각 검사하며, 반환값은 없다.
static void runSmallBoardExhaustiveBensonTest() {
  for(int boardSize = 2; boardSize <= 3; boardSize++) {
    int stateCount = 1;
    for(int i = 0; i < boardSize * boardSize; i++)
      stateCount *= 3;

    for(int state = 0; state < stateCount; state++) {
      Board board(boardSize,boardSize);
      int remaining = state;
      for(int y = 0; y < boardSize; y++) {
        for(int x = 0; x < boardSize; x++) {
          Loc loc = Location::getLoc(x,y,board.x_size);
          const int value = remaining % 3;
          remaining /= 3;
          board.colors[loc] = value == 0 ? C_EMPTY : (value == 1 ? P_BLACK : P_WHITE);
        }
      }
      board.regenChainsFromColors();

      bool hasZeroLibertyChain = false;
      for(int y = 0; y < boardSize && !hasZeroLibertyChain; y++) {
        for(int x = 0; x < boardSize; x++) {
          Loc loc = Location::getLoc(x,y,board.x_size);
          if(
            (board.colors[loc] == P_BLACK || board.colors[loc] == P_WHITE) &&
            board.getNumLiberties(loc) == 0
          ) {
            hasZeroLibertyChain = true;
            break;
          }
        }
      }
      if(hasZeroLibertyChain)
        continue;

      checkBensonAgainstKataGoArea(board,false);
      checkBensonAgainstKataGoArea(board,true);
    }
  }
}

// runEyePointFactsTest는 단일점 눈의 공개 출력 없이 내부 Board 사실만 추출하는지 검증한다.
// 반환값은 없으며 단순 눈, 거짓 눈, 따냄 진입, 혼합 경계를 각각 검사한다.
static void runEyePointFactsTest() {
  {
    Board board = Board::parseBoard(5,5,R"%%(
.....
..x..
.x.x.
..x..
.....
)%%");
    BoardHistory hist(board,P_WHITE,Rules::getTrompTaylorish(),0,false);
    vector<GoPattern::EyePointFact> facts = GoPattern::extractEyePointFacts(board,hist);
    testAssert(facts.size() == 1);
    testAssert(facts[0].owner == P_BLACK);
    testAssert(facts[0].simpleEye);
    testAssert(!facts[0].opponentEntryLegalIgnoringKo);
    testAssert(facts[0].hostileDiagonalPoints.empty());
    testAssert(facts[0].borderingChainHeads.size() == 4);
  }

  {
    Board board = Board::parseBoard(5,5,R"%%(
.....
.oxo.
.x.x.
..x..
.....
)%%");
    Loc center = Location::getLoc(2,2,board.x_size);
    BoardHistory hist(board,P_WHITE,Rules::getTrompTaylorish(),0,false);
    vector<GoPattern::EyePointFact> facts = GoPattern::extractEyePointFacts(board,hist);
    testAssert(facts.size() == 1);
    testAssert(facts[0].loc == center);
    testAssert(!facts[0].simpleEye);
    testAssert(facts[0].hostileDiagonalPoints.size() == 2);
  }

  {
    Board board = Board::parseBoard(7,7,R"%%(
.......
.ooooo.
.oxxxo.
.ox.xo.
.oxxxo.
.ooooo.
.......
)%%");
    BoardHistory hist(board,P_WHITE,Rules::getTrompTaylorish(),0,false);
    vector<GoPattern::EyePointFact> facts = GoPattern::extractEyePointFacts(board,hist);
    testAssert(facts.size() == 1);
    testAssert(facts[0].simpleEye);
    testAssert(facts[0].opponentEntryLegalIgnoringKo);
    testAssert(facts[0].borderingChainHeads.size() == 1);
  }

  {
    Board board = Board::parseBoard(5,3,R"%%(
.....
.xox.
.....
)%%");
    BoardHistory hist(board,P_BLACK,Rules::getTrompTaylorish(),0,false);
    vector<GoPattern::EyePointFact> facts = GoPattern::extractEyePointFacts(board,hist);
    testAssert(facts.empty());
  }
}

// makeNakadeFixtureBoard는 한 흑 chain 안에 지정한 빈 눈 공간만 남긴 표적 판을 만든다.
static Board makeNakadeFixtureBoard(
  int xSize,
  int ySize,
  const vector<pair<int,int>>& emptyPoints
) {
  Board board(xSize,ySize);
  for(int y = 0; y < ySize; y++) {
    for(int x = 0; x < xSize; x++) {
      Loc loc = Location::getLoc(x,y,xSize);
      board.colors[loc] = P_BLACK;
    }
  }
  for(const pair<int,int>& point: emptyPoints) {
    Loc loc = Location::getLoc(point.first,point.second,xSize);
    board.colors[loc] = C_EMPTY;
  }
  board.regenChainsFromColors();
  return board;
}

// translatedNakadePoints는 catalog 기준 좌표를 판 위의 지정 위치로 평행이동한다.
static vector<pair<int,int>> translatedNakadePoints(
  const vector<pair<int,int>>& points,
  int offsetX,
  int offsetY
) {
  vector<pair<int,int>> translated;
  translated.reserve(points.size());
  for(const pair<int,int>& point: points)
    translated.push_back(make_pair(point.first+offsetX,point.second+offsetY));
  return translated;
}

// normalizeNakadeTestShape는 테스트 도형을 평행이동·정렬해 같은 고정 방향의 키로 만든다.
static vector<pair<int,int>> normalizeNakadeTestShape(
  const vector<pair<int,int>>& points
) {
  int minX = points[0].first;
  int minY = points[0].second;
  for(const pair<int,int>& point: points) {
    minX = min(minX,point.first);
    minY = min(minY,point.second);
  }
  vector<pair<int,int>> normalized;
  normalized.reserve(points.size());
  for(const pair<int,int>& point: points)
    normalized.push_back(make_pair(point.first-minX,point.second-minY));
  sort(normalized.begin(),normalized.end());
  return normalized;
}

// transformNakadeTestShape는 테스트 입력과 급소에 정사각 격자의 한 대칭을 적용한다.
static vector<pair<int,int>> transformNakadeTestShape(
  const vector<pair<int,int>>& points,
  int symmetry
) {
  vector<pair<int,int>> transformed;
  transformed.reserve(points.size());
  for(const pair<int,int>& point: points) {
    int x = point.first;
    int y = point.second;
    switch(symmetry) {
    case 0: transformed.push_back(make_pair(x,y)); break;
    case 1: transformed.push_back(make_pair(x,-y)); break;
    case 2: transformed.push_back(make_pair(-x,y)); break;
    case 3: transformed.push_back(make_pair(-x,-y)); break;
    case 4: transformed.push_back(make_pair(y,x)); break;
    case 5: transformed.push_back(make_pair(y,-x)); break;
    case 6: transformed.push_back(make_pair(-y,x)); break;
    case 7: transformed.push_back(make_pair(-y,-x)); break;
    default: testAssert(false);
    }
  }
  return normalizeNakadeTestShape(transformed);
}

// canonicalNakadeTestShape는 자유 polyomino의 8대칭 중 사전순 최소 표현을 구한다.
static vector<pair<int,int>> canonicalNakadeTestShape(
  const vector<pair<int,int>>& points
) {
  vector<pair<int,int>> canonical = transformNakadeTestShape(points,0);
  for(int symmetry = 1; symmetry < 8; symmetry++)
    canonical = min(canonical,transformNakadeTestShape(points,symmetry));
  return canonical;
}

// enumerateFreeNakadeTestShapes는 한 점에서 인접점을 늘려 크기 3~6의 자유 polyomino를 전수 생성한다.
static vector<vector<pair<int,int>>> enumerateFreeNakadeTestShapes() {
  set<vector<pair<int,int>>> current;
  current.insert({{0,0}});
  vector<vector<pair<int,int>>> all;
  const int dx[4] = {1,-1,0,0};
  const int dy[4] = {0,0,1,-1};
  for(int size = 2; size <= 6; size++) {
    set<vector<pair<int,int>>> next;
    for(const vector<pair<int,int>>& shape: current) {
      for(const pair<int,int>& point: shape) {
        for(int direction = 0; direction < 4; direction++) {
          pair<int,int> added = make_pair(
            point.first+dx[direction],
            point.second+dy[direction]
          );
          if(find(shape.begin(),shape.end(),added) != shape.end())
            continue;
          vector<pair<int,int>> grown = shape;
          grown.push_back(added);
          next.insert(canonicalNakadeTestShape(grown));
        }
      }
    }
    current = std::move(next);
    if(size >= 3)
      all.insert(all.end(),current.begin(),current.end());
  }
  return all;
}

// runNakadeCatalogExhaustiveTest는 크기 3~6 자유 polyomino 전체에서 v1 여섯 형태만 검출되는지 대조한다.
static void runNakadeCatalogExhaustiveTest() {
  const set<vector<pair<int,int>>> supported = {
    canonicalNakadeTestShape({{0,0},{1,0},{2,0}}),
    canonicalNakadeTestShape({{0,0},{0,1},{1,0}}),
    canonicalNakadeTestShape({{0,0},{1,0},{1,1},{2,0}}),
    canonicalNakadeTestShape({{0,1},{1,0},{1,1},{1,2},{2,1}}),
    canonicalNakadeTestShape({{0,0},{0,1},{0,2},{1,0},{1,1}}),
    canonicalNakadeTestShape({{0,1},{1,0},{1,1},{1,2},{2,1},{2,2}}),
  };
  const vector<vector<pair<int,int>>> shapes = enumerateFreeNakadeTestShapes();
  testAssert(shapes.size() == 54);
  testAssert(supported.size() == 6);

  int emitted = 0;
  for(const vector<pair<int,int>>& shape: shapes) {
    vector<pair<int,int>> emptyPoints = translatedNakadePoints(shape,3,3);
    Board board = makeNakadeFixtureBoard(12,12,emptyPoints);
    BoardHistory hist(board,P_WHITE,Rules::getTrompTaylorish(),0,false);
    GoPattern::DetectionResult result = GoPattern::detect(board,hist);
    const bool actual = findKind(result.patterns,GoPattern::Kind::NAKADE) != nullptr;
    const bool expected = supported.find(shape) != supported.end();
    testAssert(actual == expected);
    if(actual)
      emitted++;
  }
  testAssert(emitted == 6);
  cout << "Nakade catalog validation: " << shapes.size()
       << " free polyominoes, " << emitted << " supported" << endl;
}

// runNakadePatternTest는 v1의 여섯 유일 급소 형태와 공개 localEyeSpace 계약을 검증한다.
static void runNakadePatternTest() {
  struct Fixture {
    GoPattern::NakadeShape shape;
    string shapeName;
    vector<pair<int,int>> points;
    pair<int,int> vitalPoint;
  };
  const vector<Fixture> fixtures = {
    {
      GoPattern::NakadeShape::STRAIGHT_THREE,
      "straightThree",
      {{0,0},{1,0},{2,0}},
      {1,0},
    },
    {
      GoPattern::NakadeShape::BENT_THREE,
      "bentThree",
      {{0,0},{0,1},{1,0}},
      {0,0},
    },
    {
      GoPattern::NakadeShape::PYRAMID_FOUR,
      "pyramidFour",
      {{0,0},{1,0},{1,1},{2,0}},
      {1,0},
    },
    {
      GoPattern::NakadeShape::STAR_FIVE,
      "starFive",
      {{0,1},{1,0},{1,1},{1,2},{2,1}},
      {1,1},
    },
    {
      GoPattern::NakadeShape::BULKY_FIVE,
      "bulkyFive",
      {{0,0},{0,1},{0,2},{1,0},{1,1}},
      {0,1},
    },
    {
      GoPattern::NakadeShape::RABBITY_SIX,
      "rabbitySix",
      {{0,1},{1,0},{1,1},{1,2},{2,1},{2,2}},
      {1,1},
    },
  };

  for(const Fixture& fixture: fixtures) {
    vector<pair<int,int>> emptyPoints = translatedNakadePoints(fixture.points,3,2);
    Board board = makeNakadeFixtureBoard(9,7,emptyPoints);
    BoardHistory hist(board,P_WHITE,Rules::getTrompTaylorish(),0,false);
    GoPattern::DetectionResult result = GoPattern::detect(board,hist);
    const GoPattern::Pattern* pattern = findKind(result.patterns,GoPattern::Kind::NAKADE);
    testAssert(pattern != nullptr);
    testAssert(pattern->schemaVersion == 1);
    testAssert(pattern->subject.type == GoPattern::SubjectType::REGION);
    testAssert(pattern->subject.color == P_BLACK);
    testAssert(pattern->subject.points.size() == fixture.points.size());
    testAssert(pattern->outcome == GoPattern::Outcome::ATTACKER_CAN_REDUCE_TO_ONE_EYE);
    testAssert(pattern->confidence == GoPattern::Confidence::HIGH);

    const GoPattern::NakadeDetails& details =
      get<GoPattern::NakadeDetails>(pattern->details);
    Loc expectedVital = Location::getLoc(
      fixture.vitalPoint.first+3,
      fixture.vitalPoint.second+2,
      board.x_size
    );
    testAssert(details.attacker == P_WHITE);
    testAssert(details.defender == P_BLACK);
    testAssert(details.defenderSubjectId == "chain:B:A1");
    testAssert(details.shape == fixture.shape);
    testAssert(details.vitalPoint == expectedVital);

    json encoded = GoPattern::toJson(*pattern,board);
    testAssert(encoded["kind"] == "nakade");
    testAssert(encoded["schemaVersion"] == 1);
    testAssert(encoded["subject"]["type"] == "region");
    testAssert(encoded["outcome"] == "attackerCanReduceToOneEye");
    testAssert(encoded["confidence"] == "high");
    testAssert(encoded["details"]["attacker"] == "W");
    testAssert(encoded["details"]["defender"] == "B");
    testAssert(encoded["details"]["defenderSubjectId"] == "chain:B:A1");
    testAssert(encoded["details"]["shape"] == fixture.shapeName);
    testAssert(
      encoded["details"]["vitalPoint"] ==
      Location::toString(expectedVital,board)
    );
    testAssert(encoded["details"]["semantics"] == "localEyeSpace");
  }

  {
    vector<pair<int,int>> points = translatedNakadePoints(fixtures[0].points,3,2);
    Board board = makeNakadeFixtureBoard(9,7,points);
    BoardHistory defenderTurn(board,P_BLACK,Rules::getTrompTaylorish(),0,false);
    GoPattern::DetectionResult defenderTurnResult =
      GoPattern::detect(board,defenderTurn);
    const GoPattern::Pattern* pattern = findKind(
      defenderTurnResult.patterns,
      GoPattern::Kind::NAKADE
    );
    testAssert(pattern != nullptr);
    testAssert(pattern->outcome == GoPattern::Outcome::UNSETTLED);
    testAssert(pattern->confidence == GoPattern::Confidence::MEDIUM);

    BoardHistory koBoundary(board,P_WHITE,Rules::getTrompTaylorish(),0,false);
    Loc vital = Location::getLoc(4,2,board.x_size);
    koBoundary.superKoBanned[vital] = true;
    GoPattern::DetectionResult koBoundaryResult =
      GoPattern::detect(board,koBoundary);
    pattern = findKind(
      koBoundaryResult.patterns,
      GoPattern::Kind::NAKADE
    );
    testAssert(pattern != nullptr);
    testAssert(pattern->outcome == GoPattern::Outcome::UNSETTLED);
    testAssert(pattern->confidence == GoPattern::Confidence::MEDIUM);
  }

  {
    vector<pair<int,int>> sidePoints = translatedNakadePoints(fixtures[2].points,0,2);
    Board sideBoard = makeNakadeFixtureBoard(7,7,sidePoints);
    BoardHistory hist(sideBoard,P_WHITE,Rules::getTrompTaylorish(),0,false);
    GoPattern::DetectionResult sideResult = GoPattern::detect(sideBoard,hist);
    testAssert(
      findKind(sideResult.patterns,GoPattern::Kind::NAKADE) !=
      nullptr
    );

    Board cornerBoard = makeNakadeFixtureBoard(7,7,fixtures[1].points);
    BoardHistory cornerHist(cornerBoard,P_WHITE,Rules::getTrompTaylorish(),0,false);
    GoPattern::DetectionResult cornerResult = GoPattern::detect(cornerBoard,cornerHist);
    testAssert(
      findKind(cornerResult.patterns,GoPattern::Kind::NAKADE) == nullptr
    );
  }

  {
    vector<pair<int,int>> squareFour = {{3,2},{3,3},{4,2},{4,3}};
    Board board = makeNakadeFixtureBoard(9,7,squareFour);
    BoardHistory hist(board,P_WHITE,Rules::getTrompTaylorish(),0,false);
    GoPattern::DetectionResult squareResult = GoPattern::detect(board,hist);
    testAssert(
      findKind(squareResult.patterns,GoPattern::Kind::NAKADE) == nullptr
    );

    vector<pair<int,int>> straightThree = {{3,2},{4,2},{5,2}};
    Board mixedBoard = makeNakadeFixtureBoard(9,7,straightThree);
    mixedBoard.colors[Location::getLoc(3,1,mixedBoard.x_size)] = P_WHITE;
    mixedBoard.regenChainsFromColors();
    BoardHistory mixedHist(mixedBoard,P_WHITE,Rules::getTrompTaylorish(),0,false);
    testAssert(
      findKind(
        GoPattern::detect(mixedBoard,mixedHist).patterns,
        GoPattern::Kind::NAKADE
      ) == nullptr
    );
  }
}

// runLadderPatternTest는 완결 승패, 기존 bool 호환, 패 의존, 예산 미완결을 분리해 검증한다.
// 반환값은 없으며 공개 Ladder v1 봉투와 내부 tri-state를 함께 검사한다.
static void runLadderPatternTest() {
  Board board = Board::parseBoard(9,9,R"%%(
xo.x..oxo
xo.o..o..
xxo......
..o.x....
xo..xo...
...ooxo..
.....xo..
xoox..xo.
.xx.o.xxo
)%%");
  BoardHistory hist(board,P_WHITE,Rules::getTrompTaylorish(),0,false);
  GoPattern::DetectionResult detection = GoPattern::detect(board,hist);
  int attackerWins = 0;
  int ambiguous = 0;
  Loc highSubjectPoint = Board::NULL_LOC;
  for(const GoPattern::Pattern& pattern: detection.patterns) {
    if(pattern.kind != GoPattern::Kind::LADDER)
      continue;
    testAssert(pattern.subject.type == GoPattern::SubjectType::CHAIN);
    const GoPattern::LadderDetails& details = get<GoPattern::LadderDetails>(pattern.details);
    testAssert(pattern.subject.color == details.defender);
    testAssert(details.attacker == getOpp(details.defender));
    testAssert(details.defenderFirst == (details.defender == P_WHITE));
    testAssert(details.ladderBreaker == Board::NULL_LOC);
    testAssert(
      board.getNumLiberties(pattern.subject.points[0]) ==
      (details.defenderFirst ? 1 : 2)
    );
    if(pattern.outcome == GoPattern::Outcome::ATTACKER_WINS) {
      testAssert(pattern.confidence == GoPattern::Confidence::HIGH);
      attackerWins += 1;
      if(highSubjectPoint == Board::NULL_LOC)
        highSubjectPoint = pattern.subject.points[0];
    }
    else if(pattern.outcome == GoPattern::Outcome::DEFENDER_ESCAPES) {
      testAssert(pattern.confidence == GoPattern::Confidence::HIGH);
      testAssert(pattern.outcome == GoPattern::Outcome::DEFENDER_ESCAPES);
      if(highSubjectPoint == Board::NULL_LOC)
        highSubjectPoint = pattern.subject.points[0];
    }
    else {
      testAssert(pattern.outcome == GoPattern::Outcome::AMBIGUOUS);
      testAssert(pattern.confidence == GoPattern::Confidence::MEDIUM);
      ambiguous += 1;
    }

    json encoded = GoPattern::toJson(pattern,board);
    testAssert(encoded["kind"] == "ladder");
    testAssert(encoded["schemaVersion"] == 1);
    testAssert(
      encoded["details"]["attacker"] ==
      (details.attacker == P_BLACK ? "B" : "W")
    );
    testAssert(
      encoded["details"]["defender"] ==
      (details.defender == P_BLACK ? "B" : "W")
    );
    testAssert(encoded["details"]["defenderFirst"] == details.defenderFirst);
    testAssert(encoded["details"]["ladderBreaker"].is_null());
  }
  testAssert(attackerWins > 0);
  testAssert(ambiguous > 0);
  testAssert(detection.warnings.empty());
  testAssert(highSubjectPoint != Board::NULL_LOC);

  BoardHistory historyBoundaryHist(hist);
  Loc historyBoundaryMove = Board::NULL_LOC;
  Loc highHead = board.chain_head[highSubjectPoint];
  Loc highStone = highHead;
  do {
    for(int i = 0; i < 4; i++) {
      Loc adjacent = highStone + board.adj_offsets[i];
      if(board.colors[adjacent] == C_EMPTY) {
        historyBoundaryMove = adjacent;
        break;
      }
    }
    highStone = board.next_in_chain[highStone];
  } while(historyBoundaryMove == Board::NULL_LOC && highStone != highHead);
  testAssert(historyBoundaryMove != Board::NULL_LOC);
  historyBoundaryHist.superKoBanned[historyBoundaryMove] = true;
  GoPattern::DetectionResult historyBoundaryDetection =
    GoPattern::detect(board,historyBoundaryHist);
  bool foundHistoryBoundary = false;
  for(const GoPattern::Pattern& pattern: historyBoundaryDetection.patterns) {
    if(pattern.kind != GoPattern::Kind::LADDER)
      continue;
    for(Loc point: pattern.subject.points) {
      if(board.chain_head[point] == highHead) {
        testAssert(pattern.outcome == GoPattern::Outcome::AMBIGUOUS);
        testAssert(pattern.confidence == GoPattern::Confidence::MEDIUM);
        foundHistoryBoundary = true;
        break;
      }
    }
  }
  testAssert(foundHistoryBoundary);

  bool checkedLegacyCompatibility = false;
  bool checkedBudget = false;
  bool checkedTacticalCaptureBoundary = false;
  for(int y = 0; y < board.y_size; y++) {
    for(int x = 0; x < board.x_size; x++) {
      Loc loc = Location::getLoc(x,y,board.x_size);
      if(
        board.colors[loc] != P_BLACK ||
        board.chain_head[loc] != loc ||
        board.getNumLiberties(loc) != 2
      )
        continue;
      Board legacyBoard(board);
      Board detailedBoard(board);
      vector<Loc> legacyBuffer;
      vector<Loc> legacyMoves;
      vector<Loc> detailedBuffer;
      vector<Loc> detailedMoves;
      bool legacyCaptured = legacyBoard.searchIsLadderCapturedAttackerFirst2Libs(
        loc,
        legacyBuffer,
        legacyMoves
      );
      LadderSearchResult detailed = detailedBoard.searchIsLadderCapturedAttackerFirst2LibsDetailed(
        loc,
        detailedBuffer,
        detailedMoves
      );
      if(detailed.status != LadderSearchStatus::UNKNOWN) {
        testAssert(
          legacyCaptured ==
          (detailed.status == LadderSearchStatus::CAPTURED)
        );
        checkedLegacyCompatibility = true;
      }
      else if(detailed.tacticalCaptureDependent) {
        checkedTacticalCaptureBoundary = true;
      }

      if(!checkedBudget) {
        Board budgetBoard(board);
        vector<Loc> budgetBuffer;
        vector<Loc> budgetMoves;
        LadderSearchResult budgetResult =
          budgetBoard.searchIsLadderCapturedAttackerFirst2LibsDetailed(
            loc,
            budgetBuffer,
            budgetMoves,
            0
          );
        testAssert(budgetResult.status == LadderSearchStatus::UNKNOWN);
        testAssert(budgetResult.budgetExhausted);
        testAssert(!budgetResult.koDependent);
        testAssert(!budgetResult.tacticalCaptureDependent);
        testAssert(!budgetResult.repetitionDependent);
        testAssert(!budgetResult.sequenceLimitReached);
        checkedBudget = true;
      }
    }
    if(checkedLegacyCompatibility && checkedBudget && checkedTacticalCaptureBoundary)
      break;
  }
  testAssert(checkedLegacyCompatibility);
  testAssert(checkedBudget);
  testAssert(checkedTacticalCaptureBoundary);

  Board koBoard = Board::parseBoard(18,9,R"%%(
..................
..................
....ox.......ox...
..xooox....xooox..
..xoxox....xoxox..
..xx.x......x.x...
...ox.......ox....
....o.............
..................
)%%");
  bool foundKoDependent = false;
  for(int y = 0; y < koBoard.y_size && !foundKoDependent; y++) {
    for(int x = 0; x < koBoard.x_size; x++) {
      Loc loc = Location::getLoc(x,y,koBoard.x_size);
      if(
        (koBoard.colors[loc] != P_BLACK && koBoard.colors[loc] != P_WHITE) ||
        koBoard.chain_head[loc] != loc ||
        koBoard.getNumLiberties(loc) != 2
      )
        continue;
      Board workingBoard(koBoard);
      vector<Loc> buffer;
      vector<Loc> workingMoves;
      LadderSearchResult result = workingBoard.searchIsLadderCapturedAttackerFirst2LibsDetailed(
        loc,
        buffer,
        workingMoves
      );
      if(result.status == LadderSearchStatus::UNKNOWN && result.koDependent)
        foundKoDependent = true;
    }
  }
  testAssert(foundKoDependent);
}

// playRandomLegalMove는 현재 차례에 둘 수 있는 보드 위 수 중 하나를 결정적으로 골라 적용한다.
// board와 hist는 수정할 게임 상태, nextPla는 현재 차례, rand는 고정 seed 난수이며, 반환값은 착수 성공 여부다.
static bool playRandomLegalMove(Board& board, BoardHistory& hist, Player& nextPla, Rand& rand) {
  vector<Loc> legalMoves;
  for(int y = 0; y < board.y_size; y++) {
    for(int x = 0; x < board.x_size; x++) {
      Loc loc = Location::getLoc(x,y,board.x_size);
      if(hist.isLegal(board,loc,nextPla))
        legalMoves.push_back(loc);
    }
  }
  if(legalMoves.empty())
    return false;

  Loc moveLoc = legalMoves[rand.nextUInt(static_cast<uint32_t>(legalMoves.size()))];
  hist.makeBoardMoveAssumeLegal(board,moveLoc,nextPla,NULL);
  nextPla = getOpp(nextPla);
  return true;
}

// randomLegalPosition은 빈 판에서 합법 수순을 재생해 property 검증용 상태를 만든다.
// xSize와 ySize는 보드 크기, suicideLegal은 룰, targetMoves는 최대 착수 수, rand는 고정 seed 난수다.
// 반환값은 보드와 일치하는 history이며 board는 최종 상태로 수정된다.
static BoardHistory randomLegalPosition(
  Board& board,
  int xSize,
  int ySize,
  bool suicideLegal,
  int targetMoves,
  Rand& rand
) {
  board = Board(xSize,ySize);
  Rules rules = Rules::getTrompTaylorish();
  rules.multiStoneSuicideLegal = suicideLegal;
  Player nextPla = P_BLACK;
  BoardHistory hist(board,nextPla,rules,0,false);
  for(int i = 0; i < targetMoves; i++) {
    if(!playRandomLegalMove(board,hist,nextPla,rand))
      break;
  }
  return hist;
}

enum class LadderOracleStatus {
  CAPTURED,
  ESCAPED,
  UNKNOWN,
};

// appendUniqueLoc는 테스트 oracle의 좌표 목록에 중복 없는 값만 추가한다.
static void appendUniqueLoc(vector<Loc>& values, Loc value) {
  if(find(values.begin(),values.end(),value) == values.end())
    values.push_back(value);
}

// oracleChainLiberties는 Board 공개 연결 정보만으로 대상 chain의 활로를 독립 열거한다.
static vector<Loc> oracleChainLiberties(const Board& board, Loc anchor) {
  vector<Loc> liberties;
  if(board.colors[anchor] != P_BLACK && board.colors[anchor] != P_WHITE)
    return liberties;
  Loc head = board.chain_head[anchor];
  Loc stone = head;
  do {
    for(int i = 0; i < 4; i++) {
      Loc adj = stone + board.adj_offsets[i];
      if(board.colors[adj] == C_EMPTY)
        appendUniqueLoc(liberties,adj);
    }
    stone = board.next_in_chain[stone];
  } while(stone != head);
  return liberties;
}

// oracleDefenderMoves는 목표 chain의 활로와 인접 단수 상대군 따냄 수를 독립 열거한다.
static vector<Loc> oracleDefenderMoves(const Board& board, Loc anchor, Player defender) {
  vector<Loc> moves = oracleChainLiberties(board,anchor);
  Loc targetHead = board.chain_head[anchor];
  Loc stone = targetHead;
  do {
    for(int i = 0; i < 4; i++) {
      Loc adjacent = stone + board.adj_offsets[i];
      if(
        board.colors[adjacent] != getOpp(defender) ||
        board.getNumLiberties(adjacent) != 1
      )
        continue;
      vector<Loc> captureLiberties = oracleChainLiberties(board,adjacent);
      testAssert(captureLiberties.size() == 1);
      appendUniqueLoc(moves,captureLiberties[0]);
    }
    stone = board.next_in_chain[stone];
  } while(stone != targetHead);
  return moves;
}

// solveLadderOracle는 작은 판에서 활로·인접 단수 따냄만 생성하는 독립 minimax로 사다리를 판정한다.
static LadderOracleStatus solveLadderOracle(
  const Board& board,
  Loc anchor,
  Player defender,
  bool defenderTurn,
  int remainingDepth,
  vector<Hash128>& path
) {
  if(board.colors[anchor] != defender)
    return LadderOracleStatus::CAPTURED;
  const int liberties = board.getNumLiberties(anchor);
  if(defenderTurn && liberties >= 2)
    return LadderOracleStatus::ESCAPED;
  if(!defenderTurn && liberties <= 1)
    return LadderOracleStatus::CAPTURED;
  if(!defenderTurn && liberties >= 3)
    return LadderOracleStatus::ESCAPED;
  if(remainingDepth <= 0)
    return LadderOracleStatus::UNKNOWN;

  Player nextPlayer = defenderTurn ? defender : getOpp(defender);
  Hash128 situation = board.getSitHashWithSimpleKo(nextPlayer);
  if(find(path.begin(),path.end(),situation) != path.end())
    return LadderOracleStatus::UNKNOWN;
  path.push_back(situation);

  vector<Loc> moves = defenderTurn ?
    oracleDefenderMoves(board,anchor,defender) :
    oracleChainLiberties(board,anchor);
  bool foundLegalMove = false;
  bool foundUnknown = false;
  for(Loc move: moves) {
    if(move == board.ko_loc) {
      foundUnknown = true;
      continue;
    }
    Board child(board);
    if(!child.playMove(move,nextPlayer,false))
      continue;
    foundLegalMove = true;
    LadderOracleStatus childStatus = solveLadderOracle(
      child,
      anchor,
      defender,
      !defenderTurn,
      remainingDepth - 1,
      path
    );
    if(defenderTurn && childStatus == LadderOracleStatus::ESCAPED) {
      path.pop_back();
      return LadderOracleStatus::ESCAPED;
    }
    if(!defenderTurn && childStatus == LadderOracleStatus::CAPTURED) {
      path.pop_back();
      return LadderOracleStatus::CAPTURED;
    }
    if(childStatus == LadderOracleStatus::UNKNOWN)
      foundUnknown = true;
  }
  path.pop_back();

  if(foundUnknown)
    return LadderOracleStatus::UNKNOWN;
  if(!foundLegalMove)
    return defenderTurn ? LadderOracleStatus::CAPTURED : LadderOracleStatus::ESCAPED;
  return defenderTurn ? LadderOracleStatus::CAPTURED : LadderOracleStatus::ESCAPED;
}

// runLadderIndependentPropertyTest는 작은 합법 상태에서 상세 탐색을 독립 minimax와 500건 대조한다.
// 반환값은 검증한 완결 후보 수이며 captured·escaped를 각각 200건 이상 포함한다.
static int runLadderIndependentPropertyTest() {
  Rand rand("ladder-independent-property-20260807");
  int checked = 0;
  int captured = 0;
  int escaped = 0;
  int applicable = 0;
  int emitted = 0;
  int validatedStates = 0;
  int lastValidatedSample = -1;
  int nonSquareChecked = 0;
  int defenderFirstChecked = 0;
  int attackerFirstChecked = 0;
  bool validatedEscapedSerialization = false;
  for(int sample = 0; sample < 20000; sample++) {
    int xSize = 4 + static_cast<int>(rand.nextUInt(4));
    int ySize = 4 + static_cast<int>(rand.nextUInt(4));
    int area = xSize * ySize;
    Board board;
    int targetMoves = static_cast<int>(rand.nextUInt(static_cast<uint32_t>(area * 2 + 1)));
    BoardHistory generatedHist =
      randomLegalPosition(board,xSize,ySize,sample % 2 == 1,targetMoves,rand);

    for(int y = 0; y < board.y_size; y++) {
      for(int x = 0; x < board.x_size; x++) {
        Loc loc = Location::getLoc(x,y,board.x_size);
        Player defender = board.colors[loc];
        if(
          (defender != P_BLACK && defender != P_WHITE) ||
          board.chain_head[loc] != loc
        )
          continue;
        int liberties = board.getNumLiberties(loc);
        if(liberties != 1 && liberties != 2)
          continue;
        applicable += 1;

        bool defenderTurn = liberties == 1;
        Board detailedBoard(board);
        vector<Loc> buffer;
        vector<Loc> workingMoves;
        LadderSearchResult detailed = defenderTurn ?
          detailedBoard.searchIsLadderCapturedDetailed(loc,true,buffer) :
          detailedBoard.searchIsLadderCapturedAttackerFirst2LibsDetailed(
            loc,
            buffer,
            workingMoves
          );
        if(!detailed.budgetExhausted && (
          detailed.status != LadderSearchStatus::UNKNOWN ||
          detailed.koDependent ||
          detailed.tacticalCaptureDependent ||
          detailed.repetitionDependent ||
          detailed.sequenceLimitReached
        ))
          emitted += 1;
        if(detailed.status == LadderSearchStatus::UNKNOWN)
          continue;

        vector<Hash128> path;
        LadderOracleStatus oracle = solveLadderOracle(
          board,
          loc,
          defender,
          defenderTurn,
          area * 2,
          path
        );
        if(oracle == LadderOracleStatus::UNKNOWN)
          continue;
        bool detailedCaptured = detailed.status == LadderSearchStatus::CAPTURED;
        bool oracleCaptured = oracle == LadderOracleStatus::CAPTURED;
        testAssert(detailedCaptured == oracleCaptured);
        checked += 1;
        if(lastValidatedSample != sample) {
          validatedStates += 1;
          lastValidatedSample = sample;
        }
        if(xSize != ySize)
          nonSquareChecked += 1;
        if(defenderTurn)
          defenderFirstChecked += 1;
        else
          attackerFirstChecked += 1;
        if(detailedCaptured)
          captured += 1;
        else {
          escaped += 1;
          if(!validatedEscapedSerialization) {
            Player nextPlayer = defenderTurn ? defender : getOpp(defender);
            BoardHistory outputHist(
              board,
              nextPlayer,
              generatedHist.rules,
              generatedHist.initialTurnNumber,
              false
            );
            GoPattern::DetectionResult detection = GoPattern::detect(board,outputHist);
            bool foundEscaped = false;
            for(const GoPattern::Pattern& pattern: detection.patterns) {
              if(
                pattern.kind != GoPattern::Kind::LADDER ||
                pattern.outcome != GoPattern::Outcome::DEFENDER_ESCAPES
              )
                continue;
              for(Loc point: pattern.subject.points) {
                if(board.chain_head[point] == board.chain_head[loc]) {
                  json encoded = GoPattern::toJson(pattern,board);
                  testAssert(encoded["outcome"] == "defenderEscapes");
                  testAssert(encoded["confidence"] == "high");
                  foundEscaped = true;
                  break;
                }
              }
              if(foundEscaped)
                break;
            }
            testAssert(foundEscaped);
            validatedEscapedSerialization = true;
          }
        }
        if(
          checked >= 500 &&
          captured >= 200 &&
          escaped >= 200 &&
          nonSquareChecked >= 100 &&
          defenderFirstChecked >= 100 &&
          attackerFirstChecked >= 100 &&
          validatedEscapedSerialization &&
          emitted * 100 >= applicable * 90
        ) {
          cout
            << "Ladder independent validation: "
            << checked << " candidates, "
            << captured << " captured, "
            << escaped << " escaped, "
            << validatedStates << " states, "
            << nonSquareChecked << " non-square, "
            << emitted << "/" << applicable << " emitted"
            << endl;
          return checked;
        }
      }
    }
  }
  testAssert(checked >= 500);
  testAssert(captured >= 200);
  testAssert(escaped >= 200);
  testAssert(nonSquareChecked >= 100);
  testAssert(defenderFirstChecked >= 100);
  testAssert(attackerFirstChecked >= 100);
  testAssert(emitted * 100 >= applicable * 90);
  testAssert(validatedEscapedSerialization);
  return checked;
}

// runSquareBoardPropertyTest는 4×4~19×19 합법 수순 상태를 두 suicide 모드로 대조한다.
// 반환값은 검증한 서로 다른 생성 상태의 수다.
static int runSquareBoardPropertyTest() {
  Rand rand("benson-square-property-20260807");
  int checkedStates = 0;
  for(int boardSize = 4; boardSize <= 19; boardSize++) {
    const int statesPerMode = boardSize <= 9 ? 500 : 50;
    const int area = boardSize * boardSize;
    for(int suicideMode = 0; suicideMode < 2; suicideMode++) {
      for(int sample = 0; sample < statesPerMode; sample++) {
        Board board;
        int targetMoves = static_cast<int>(rand.nextUInt(static_cast<uint32_t>(area * 2 + 1)));
        randomLegalPosition(board,boardSize,boardSize,suicideMode == 1,targetMoves,rand);
        checkBensonAgainstKataGoArea(board,suicideMode == 1);
        checkedStates += 1;
      }
    }
  }
  testAssert(checkedStates == 7000);
  return checkedStates;
}

// runNonSquarePropertyTest는 서로 다른 크기·seed의 비정사각 합법 게임 100개를 여러 시점에서 대조한다.
// 반환값은 검증한 게임 수와 상태 수이며 각 게임은 최소 초기 상태와 최종 상태를 포함한다.
static pair<int,int> runNonSquarePropertyTest() {
  Rand rand("benson-nonsquare-property-20260807");
  int checkedGames = 0;
  int checkedStates = 0;
  for(int gameIndex = 0; gameIndex < 100; gameIndex++) {
    int xSize = 4 + (gameIndex * 5) % 16;
    int ySize = 4 + (gameIndex * 11 + 3) % 16;
    if(xSize == ySize)
      ySize = ySize == 19 ? 18 : ySize + 1;

    const bool suicideLegal = gameIndex % 2 == 1;
    Board board(xSize,ySize);
    Rules rules = Rules::getTrompTaylorish();
    rules.multiStoneSuicideLegal = suicideLegal;
    Player nextPla = P_BLACK;
    BoardHistory hist(board,nextPla,rules,0,false);
    checkBensonAgainstKataGoArea(board,suicideLegal);
    checkedStates += 1;

    const int targetMoves = xSize * ySize;
    const int sampleInterval = max(1,targetMoves / 4);
    for(int moveIndex = 0; moveIndex < targetMoves; moveIndex++) {
      if(!playRandomLegalMove(board,hist,nextPla,rand))
        break;
      if((moveIndex + 1) % sampleInterval == 0) {
        checkBensonAgainstKataGoArea(board,suicideLegal);
        checkedStates += 1;
      }
    }
    checkBensonAgainstKataGoArea(board,suicideLegal);
    checkedStates += 1;
    checkedGames += 1;
  }
  testAssert(checkedGames == 100);
  testAssert(checkedStates >= 500);
  return {checkedGames,checkedStates};
}

// transformedBoard는 회전·반사와 선택적 색 교환으로 동치 fixture를 만든다.
// source는 원본, symmetry는 0~7, swapColors는 흑백 교환 여부이며, 반환값은 chain이 재생성된 보드다.
static Board transformedBoard(const Board& source, int symmetry, bool swapColors) {
  testAssert(symmetry >= 0 && symmetry < 8);
  const int rotation = symmetry % 4;
  const bool mirror = symmetry >= 4;
  const bool swapAxes = rotation == 1 || rotation == 3;
  Board transformed(
    swapAxes ? source.y_size : source.x_size,
    swapAxes ? source.x_size : source.y_size
  );

  for(int y = 0; y < source.y_size; y++) {
    for(int x = 0; x < source.x_size; x++) {
      int mirroredX = mirror ? source.x_size - 1 - x : x;
      int transformedX;
      int transformedY;
      if(rotation == 0) {
        transformedX = mirroredX;
        transformedY = y;
      }
      else if(rotation == 1) {
        transformedX = source.y_size - 1 - y;
        transformedY = mirroredX;
      }
      else if(rotation == 2) {
        transformedX = source.x_size - 1 - mirroredX;
        transformedY = source.y_size - 1 - y;
      }
      else {
        transformedX = y;
        transformedY = source.x_size - 1 - mirroredX;
      }

      Loc sourceLoc = Location::getLoc(x,y,source.x_size);
      Color color = source.colors[sourceLoc];
      if(swapColors && color == P_BLACK)
        color = P_WHITE;
      else if(swapColors && color == P_WHITE)
        color = P_BLACK;
      Loc transformedLoc = Location::getLoc(transformedX,transformedY,transformed.x_size);
      transformed.colors[transformedLoc] = color;
    }
  }
  transformed.regenChainsFromColors();
  return transformed;
}

// areaDiffersBySuicide는 pass-alive 결과가 multi-stone suicide 모드에 실제로 민감한지 검사한다.
// board는 대상 상태이며, 반환값은 한 점이라도 두 모드의 결과가 다르면 true다.
static bool areaDiffersBySuicide(const Board& board) {
  Color suicideIllegal[Board::MAX_ARR_SIZE];
  Color suicideLegal[Board::MAX_ARR_SIZE];
  board.calculateArea(suicideIllegal,false,false,false,false);
  board.calculateArea(suicideLegal,false,false,false,true);
  for(int y = 0; y < board.y_size; y++) {
    for(int x = 0; x < board.x_size; x++) {
      Loc loc = Location::getLoc(x,y,board.x_size);
      if(suicideIllegal[loc] != suicideLegal[loc])
        return true;
    }
  }
  return false;
}

// runSuicideSensitiveFixtureTest는 suicide 모드에 따라 실제 pass-alive 결과가 달라지는 상태 30개를 대조한다.
// 반환값은 중복 제거 후 검증한 mode-sensitive fixture 수다.
static int runSuicideSensitiveFixtureTest() {
  Board source = Board::parseBoard(4,5,R"%%(
...x
xx..
oxxx
oox.
.x.x
)%%");

  vector<Board> fixtures;
  set<string> fingerprints;
  auto addIfSensitive = [&fixtures,&fingerprints](const Board& candidate) {
    if(!areaDiffersBySuicide(candidate))
      return;
    string fingerprint =
      Global::strprintf("%dx%d:",candidate.x_size,candidate.y_size) +
      Board::toStringSimple(candidate,'|');
    if(fingerprints.insert(fingerprint).second)
      fixtures.push_back(candidate);
  };

  for(int symmetry = 0; symmetry < 8; symmetry++) {
    for(int colorMode = 0; colorMode < 2; colorMode++) {
      Board transformed = transformedBoard(source,symmetry,colorMode == 1);
      addIfSensitive(transformed);
      for(int y = 0; y < transformed.y_size; y++) {
        for(int x = 0; x < transformed.x_size; x++) {
          Loc loc = Location::getLoc(x,y,transformed.x_size);
          if(transformed.colors[loc] != C_EMPTY)
            continue;
          for(Player pla: {P_BLACK,P_WHITE}) {
            Board variation(transformed);
            if(variation.setStoneFailIfNoLibs(loc,pla))
              addIfSensitive(variation);
          }
        }
      }
    }
  }

  testAssert(fixtures.size() >= 30);
  for(int i = 0; i < 30; i++) {
    checkBensonAgainstKataGoArea(fixtures[i],false);
    checkBensonAgainstKataGoArea(fixtures[i],true);
  }
  return 30;
}

// runSekiBoundaryFixtureTest는 KataGo 기존 seki-liberty 표적 국면의 네 진행 상태를 대칭 변환해 대조한다.
// 반환값은 4개 진행 상태 × 8개 대칭으로 만든 서로 다른 fixture 수다.
static int runSekiBoundaryFixtureTest() {
  Board board = Board::parseBoard(7,7,R"%%(
...oxx.
oooox.x
xxxxoxx
o.xoooo
.oxox.o
oxxo.x.
o.xoo.x
)%%");
  Rules rules;
  rules.koRule = Rules::KO_POSITIONAL;
  rules.scoringRule = Rules::SCORING_TERRITORY;
  rules.komi = -0.5f;
  rules.multiStoneSuicideLegal = false;
  BoardHistory hist(board,P_WHITE,rules,0,false);
  const vector<pair<Loc,Player>> fillMoves = {
    {Location::getLoc(6,5,board.x_size),P_WHITE},
    {Location::getLoc(5,6,board.x_size),P_BLACK},
    {Location::getLoc(0,4,board.x_size),P_WHITE},
  };

  int checkedFixtures = 0;
  for(size_t progression = 0; progression <= fillMoves.size(); progression++) {
    for(int symmetry = 0; symmetry < 8; symmetry++) {
      Board transformed = transformedBoard(board,symmetry,false);
      checkBensonAgainstKataGoArea(transformed,false);
      checkBensonAgainstKataGoArea(transformed,true);
      checkedFixtures += 1;
    }
    if(progression < fillMoves.size()) {
      Loc moveLoc = fillMoves[progression].first;
      Player movePla = fillMoves[progression].second;
      testAssert(hist.isLegal(board,moveLoc,movePla));
      hist.makeBoardMoveAssumeLegal(board,moveLoc,movePla,NULL);
    }
  }
  testAssert(checkedFixtures == 32);
  return checkedFixtures;
}

// runBensonPerformanceTest는 19×19 다중 chain 입력에서 구조 후처리 p95 예산을 확인한다.
// 반환값은 없으며 200회 중 95백분위가 20ms를 넘으면 테스트를 실패시킨다.
static void runBensonPerformanceTest() {
  Board board(19,19);
  for(int y = 0; y < board.y_size; y += 2) {
    for(int x = 0; x < board.x_size; x += 2) {
      Loc loc = Location::getLoc(x,y,board.x_size);
      board.colors[loc] = P_BLACK;
    }
  }
  board.regenChainsFromColors();
  BoardHistory hist(board,P_WHITE,Rules::getTrompTaylorish(),0,false);

  vector<double> milliseconds;
  milliseconds.reserve(200);
  for(int i = 0; i < 200; i++) {
    auto start = chrono::steady_clock::now();
    GoPattern::DetectionResult result = GoPattern::detect(board,hist);
    auto end = chrono::steady_clock::now();
    testAssert(!result.truncated);
    milliseconds.push_back(chrono::duration<double,std::milli>(end-start).count());
  }
  sort(milliseconds.begin(),milliseconds.end());
  const double p95 = milliseconds[189];
  cout << "BensonAlive 19x19 postprocess p95: " << p95 << " ms" << endl;
  testAssert(p95 <= 20.0);
}

void Tests::runPatternTests() {
  cout << "Running pattern tests" << endl;

  {
    Board board = Board::parseBoard(7,3,R"%%(
xxxxxxx
x..x..x
xxxxxxx
)%%");
    BoardHistory hist(board,P_WHITE,Rules::getTrompTaylorish(),0,false);
    GoPattern::DetectionResult result = GoPattern::detect(board,hist);
    const GoPattern::Pattern* pattern = findKind(result.patterns,GoPattern::Kind::BENSON_ALIVE);
    testAssert(pattern != nullptr);
    testAssert(!result.truncated);

    json actual = GoPattern::toJson(*pattern,board);
    json expected = {
      {"kind", "bensonAlive"},
      {"schemaVersion", 2},
      {"subject", {
        {"type", "chain"},
        {"color", "B"},
        {"id", "chain:B:A1"},
        {"points", {
          "A1","A2","A3","B1","B3","C1","C3","D1","D2",
          "D3","E1","E3","F1","F3","G1","G2","G3"
        }},
      }},
      {"outcome", "alive"},
      {"confidence", "proven"},
      {"details", {
        {"eyeCount", 2},
        {"vitalRegions", {
          {
            {"id", "region:B:B2"},
            {"points", {"B2","C2"}},
          },
          {
            {"id", "region:B:E2"},
            {"points", {"E2","F2"}},
          },
        }},
      }},
    };
    testAssert(actual == expected);
  }

  {
    Board board = Board::parseBoard(3,3,R"%%(
xxx
x.x
xxx
)%%");
    BoardHistory hist(board,P_WHITE,Rules::getTrompTaylorish(),0,false);
    GoPattern::DetectionResult result = GoPattern::detect(board,hist);
    testAssert(findKind(result.patterns,GoPattern::Kind::BENSON_ALIVE) == nullptr);
  }

  {
    Board board = Board::parseBoard(5,3,R"%%(
ooooo
o.o.o
ooooo
)%%");
    BoardHistory hist(board,P_BLACK,Rules::getTrompTaylorish(),0,false);
    GoPattern::DetectionResult result = GoPattern::detect(board,hist);
    const GoPattern::Pattern* pattern = findKind(result.patterns,GoPattern::Kind::BENSON_ALIVE);
    testAssert(pattern != nullptr);
    testAssert(pattern->subject.color == P_WHITE);
    testAssert(pattern->subject.id == "chain:W:A1");
  }

  runSmallBoardExhaustiveBensonTest();
  runEyePointFactsTest();
  runNakadePatternTest();
  runNakadeCatalogExhaustiveTest();
  runLadderPatternTest();
  runLadderIndependentPropertyTest();
  int squareStates = runSquareBoardPropertyTest();
  pair<int,int> nonSquareResult = runNonSquarePropertyTest();
  int suicideFixtures = runSuicideSensitiveFixtureTest();
  int sekiFixtures = runSekiBoundaryFixtureTest();
  cout
    << "BensonAlive validation: "
    << squareStates << " square states, "
    << nonSquareResult.first << " non-square games/"
    << nonSquareResult.second << " states, "
    << suicideFixtures << " suicide-sensitive fixtures, "
    << sekiFixtures << " seki-boundary fixtures"
    << endl;
  runBensonPerformanceTest();
}
