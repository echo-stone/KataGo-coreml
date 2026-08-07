#include "../tests/tests.h"
#include "../pattern/detector.h"
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
