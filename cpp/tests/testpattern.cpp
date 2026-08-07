#include "../tests/tests.h"
#include "../pattern/detector.h"

#include <algorithm>
#include <chrono>

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
    Board board = Board::parseBoard(5,3,R"%%(
xxxxx
x.x.x
xxxxx
)%%");
    BoardHistory hist(board,P_WHITE,Rules::getTrompTaylorish(),0,false);
    GoPattern::DetectionResult result = GoPattern::detect(board,hist);
    const GoPattern::Pattern* pattern = findKind(result.patterns,GoPattern::Kind::BENSON_ALIVE);
    testAssert(pattern != nullptr);
    testAssert(!result.truncated);

    json actual = GoPattern::toJson(*pattern,board);
    json expected = {
      {"kind", "bensonAlive"},
      {"schemaVersion", 1},
      {"subject", {
        {"type", "chain"},
        {"color", "B"},
        {"id", "chain:B:A1"},
        {"points", {"A1","A2","A3","B1","B3","C1","C2","C3","D1","D3","E1","E2","E3"}},
      }},
      {"outcome", "alive"},
      {"confidence", "proven"},
      {"details", {
        {"vitalRegionIds", {"region:B:B2","region:B:D2"}},
        {"eyeCount", 2},
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
  runBensonPerformanceTest();
}
