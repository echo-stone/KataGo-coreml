#include "../tests/tests.h"
#include "../pattern/detector.h"
#include "../pattern/eye.h"
#include "../pattern/eyefacts.h"
#include "../pattern/semeai.h"
#include "../core/rand.h"
#include "../core/sha2.h"

#include <algorithm>
#include <chrono>
#include <iterator>
#include <set>
#include <sstream>

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

// findEyeAt은 지정 한 점을 subject로 하는 공개 Eye 패턴을 찾는다.
// patterns는 검색할 결과, loc은 대상 좌표이며, 반환값은 없으면 nullptr이다.
static const GoPattern::Pattern* findEyeAt(
  const vector<GoPattern::Pattern>& patterns,
  Loc loc
) {
  for(const GoPattern::Pattern& pattern: patterns) {
    if(
      pattern.kind == GoPattern::Kind::EYE &&
      pattern.subject.points.size() == 1 &&
      pattern.subject.points[0] == loc
    )
      return &pattern;
  }
  return nullptr;
}

// makeKoEyeBoard는 백의 지정 착수 뒤 흑의 즉시 되따냄이 한 점 Eye 후보가 되는 3×3 패를 만든다.
// 반환값은 무활로 chain이 없는 고정 시작 보드다.
static Board makeKoEyeBoard() {
  Board board(3,3);
  board.colors[Location::getLoc(1,1,board.x_size)] = P_BLACK;
  board.colors[Location::getLoc(2,1,board.x_size)] = P_WHITE;
  board.colors[Location::getLoc(0,2,board.x_size)] = P_BLACK;
  board.colors[Location::getLoc(2,2,board.x_size)] = P_BLACK;
  board.regenChainsFromColors();
  return board;
}

// runEyeHistoryBoundaryTest는 실제 BoardHistory의 패·종국·encore 경계에서 Eye 공개 여부를 검증한다.
// 반환값은 없으며 계약의 합법성·pass-for-ko·phase 억제 조건을 각각 고정한다.
static void runEyeHistoryBoundaryTest() {
  const Loc koCapture = Location::getLoc(1,2,3);
  const Loc koRecapture = Location::getLoc(2,2,3);
  for(int koRule: {Rules::KO_SIMPLE,Rules::KO_POSITIONAL,Rules::KO_SITUATIONAL}) {
    Board board = makeKoEyeBoard();
    Rules rules = Rules::getSimpleTerritory();
    rules.koRule = koRule;
    BoardHistory hist(board,P_WHITE,rules,0,false);
    testAssert(hist.isLegal(board,koCapture,P_WHITE));
    testAssert(findEyeAt(GoPattern::detectEyes(board,hist),koCapture) != nullptr);
    hist.makeBoardMoveAssumeLegal(board,koCapture,P_WHITE,NULL);
    testAssert(!hist.isLegal(board,koRecapture,P_BLACK));
    testAssert(GoPattern::detectEyes(board,hist).empty());
  }

  Board positive = Board::parseBoard(7,7,R"%%(
.......
.ooooo.
.oxxxo.
.ox.xo.
.oxxxo.
.ooooo.
.......
)%%");
  const Loc center = Location::ofString("D4",positive);
  Rules territoryRules = Rules::getSimpleTerritory();
  BoardHistory encoreHist(positive,P_WHITE,territoryRules,0,false);
  encoreHist.makeBoardMoveAssumeLegal(positive,Board::PASS_LOC,P_WHITE,NULL);
  encoreHist.makeBoardMoveAssumeLegal(positive,Board::PASS_LOC,P_BLACK,NULL);
  testAssert(encoreHist.encorePhase == 1);
  testAssert(!encoreHist.isGameFinished);
  testAssert(!encoreHist.isPastNormalPhaseEnd);
  testAssert(findEyeAt(GoPattern::detectEyes(positive,encoreHist),center) != nullptr);

  Board encoreKoBoard = makeKoEyeBoard();
  BoardHistory encoreKoHist(encoreKoBoard,P_WHITE,territoryRules,0,false);
  encoreKoHist.makeBoardMoveAssumeLegal(encoreKoBoard,Board::PASS_LOC,P_WHITE,NULL);
  encoreKoHist.makeBoardMoveAssumeLegal(encoreKoBoard,Board::PASS_LOC,P_BLACK,NULL);
  testAssert(encoreKoHist.encorePhase == 1);
  testAssert(encoreKoHist.isLegal(encoreKoBoard,koCapture,P_WHITE));
  encoreKoHist.makeBoardMoveAssumeLegal(encoreKoBoard,koCapture,P_WHITE,NULL);
  testAssert(encoreKoHist.isLegal(encoreKoBoard,koRecapture,P_BLACK));
  testAssert(encoreKoHist.isPassForKo(encoreKoBoard,koRecapture,P_BLACK));
  testAssert(GoPattern::detectEyes(encoreKoBoard,encoreKoHist).empty());

  Board finishedBoard(positive);
  BoardHistory finished(finishedBoard,P_WHITE,Rules::getTrompTaylorish(),0,false);
  finished.makeBoardMoveAssumeLegal(finishedBoard,Board::PASS_LOC,P_WHITE,NULL);
  finished.makeBoardMoveAssumeLegal(finishedBoard,Board::PASS_LOC,P_BLACK,NULL);
  testAssert(finished.isGameFinished);
  testAssert(GoPattern::detectEyes(finishedBoard,finished).empty());

  Board pastNormalBoard(positive);
  BoardHistory pastNormal(pastNormalBoard,P_WHITE,territoryRules,0,false);
  pastNormal.makeBoardMoveAssumeLegal(pastNormalBoard,Board::PASS_LOC,P_WHITE,NULL);
  pastNormal.makeBoardMoveAssumeLegal(
    pastNormalBoard,
    Board::PASS_LOC,
    P_BLACK,
    NULL,
    true
  );
  testAssert(pastNormal.isPastNormalPhaseEnd);
  testAssert(GoPattern::detectEyes(pastNormalBoard,pastNormal).empty());
}

// runEyeSnapbackFixtureTest는 즉시 포획 뒤 상대가 한 돌만 되잡는 고정 snapback 전이를 검증한다.
// 반환값은 없으며 포획 chain 전체 좌표와 비반복 후속 국면을 함께 고정한다.
static void runEyeSnapbackFixtureTest() {
  Board board = Board::parseBoard(4,3,R"%%(
....
oxx.
.oox
)%%");
  const Loc capture = Location::ofString("A1",board);
  const Loc recapture = Location::ofString("B1",board);
  const Loc remainingEmpty = Location::ofString("C1",board);
  BoardHistory hist(board,P_BLACK,Rules::getSimpleTerritory(),0,false);
  const Hash128 originalHash = board.pos_hash;

  vector<GoPattern::Pattern> patterns = GoPattern::detectEyes(board,hist);
  const GoPattern::Pattern* pattern = findEyeAt(patterns,capture);
  testAssert(pattern != nullptr);
  const GoPattern::EyeDetails& details = get<GoPattern::EyeDetails>(pattern->details);
  testAssert(details.attacker == P_BLACK);
  testAssert(details.capturedChains.size() == 1);
  testAssert(details.capturedChains[0].id == "chain:W:B1");
  testAssert(details.capturedChains[0].points.size() == 2);
  testAssert(details.capturedChains[0].points[0] == recapture);
  testAssert(details.capturedChains[0].points[1] == remainingEmpty);

  testAssert(hist.isLegal(board,capture,P_BLACK));
  testAssert(!hist.isPassForKo(board,capture,P_BLACK));
  hist.makeBoardMoveAssumeLegal(board,capture,P_BLACK,NULL);
  testAssert(board.colors[recapture] == C_EMPTY);
  testAssert(board.colors[remainingEmpty] == C_EMPTY);
  testAssert(hist.isLegal(board,recapture,P_WHITE));
  testAssert(!hist.isPassForKo(board,recapture,P_WHITE));
  hist.makeBoardMoveAssumeLegal(board,recapture,P_WHITE,NULL);
  testAssert(board.colors[capture] == C_EMPTY);
  testAssert(board.colors[recapture] == P_WHITE);
  testAssert(board.colors[remainingEmpty] == C_EMPTY);
  testAssert(board.pos_hash != originalHash);
}

// runEyePatternTest는 Eye v1 typed 계약, 실제 포획, 정렬, 색 반전, 음성 경계를 검증한다.
// 반환값은 없으며 공개 JSON과 원본 불변성을 함께 고정한다.
static void runEyePatternTest() {
  Board board = Board::parseBoard(7,7,R"%%(
.......
.ooooo.
.oxxxo.
.ox.xo.
.oxxxo.
.ooooo.
.......
)%%");
  const Loc center = Location::ofString("D4",board);
  BoardHistory hist(board,P_WHITE,Rules::getTrompTaylorish(),0,false);
  const Hash128 originalHash = board.pos_hash;
  const size_t originalHistorySize = hist.moveHistory.size();
  GoPattern::DetectionResult result = GoPattern::detect(board,hist);
  const GoPattern::Pattern* pattern = findEyeAt(result.patterns,center);
  testAssert(pattern != nullptr);
  testAssert(board.pos_hash == originalHash);
  testAssert(hist.moveHistory.size() == originalHistorySize);
  testAssert(pattern->schemaVersion == 1);
  testAssert(pattern->subject.type == GoPattern::SubjectType::REGION);
  testAssert(pattern->subject.color == P_BLACK);
  testAssert(pattern->subject.id == "region:B:D4");
  testAssert(pattern->outcome == GoPattern::Outcome::OPPONENT_CAN_CAPTURE_ADJACENT_CHAIN);
  testAssert(pattern->confidence == GoPattern::Confidence::PROVEN);
  const GoPattern::EyeDetails& details = get<GoPattern::EyeDetails>(pattern->details);
  testAssert(details.attacker == P_WHITE);
  testAssert(details.capturedChains.size() == 1);
  testAssert(details.capturedChains[0].id == "chain:B:C3");

  json actual = GoPattern::toJson(*pattern,board);
  json expected = {
    {"kind", "eye"},
    {"schemaVersion", 1},
    {"subject", {
      {"type", "region"},
      {"color", "B"},
      {"id", "region:B:D4"},
      {"points", {"D4"}},
    }},
    {"outcome", "opponentCanCaptureAdjacentChain"},
    {"confidence", "proven"},
    {"details", {
      {"attacker", "W"},
      {"capturedChains", {
        {
          {"id", "chain:B:C3"},
          {"points", {"C3","C4","C5","D3","D5","E3","E4","E5"}},
        },
      }},
      {"semantics", "immediateLegalCapture"},
    }},
  };
  testAssert(actual == expected);

  Board noOwnerTurn(board);
  BoardHistory ownerTurn(noOwnerTurn,P_BLACK,Rules::getTrompTaylorish(),0,false);
  testAssert(findEyeAt(GoPattern::detectEyes(noOwnerTurn,ownerTurn),center) == nullptr);

  Board multi = Board::parseBoard(5,3,R"%%(
.oxo.
ox.xo
.oxo.
)%%");
  const Loc multiCenter = Location::ofString("C2",multi);
  BoardHistory multiHist(multi,P_WHITE,Rules::getTrompTaylorish(),0,false);
  vector<GoPattern::Pattern> multiPatterns = GoPattern::detectEyes(multi,multiHist);
  const GoPattern::Pattern* multiPattern = findEyeAt(
    multiPatterns,
    multiCenter
  );
  testAssert(multiPattern != nullptr);
  const GoPattern::EyeDetails& multiDetails = get<GoPattern::EyeDetails>(multiPattern->details);
  testAssert(multiDetails.capturedChains.size() == 4);
  testAssert(multiDetails.capturedChains[0].id == "chain:B:B2");
  testAssert(multiDetails.capturedChains[1].id == "chain:B:C1");
  testAssert(multiDetails.capturedChains[2].id == "chain:B:C3");
  testAssert(multiDetails.capturedChains[3].id == "chain:B:D2");

  Board inverted(multi.x_size,multi.y_size);
  for(int y = 0; y < multi.y_size; y++) {
    for(int x = 0; x < multi.x_size; x++) {
      Loc loc = Location::getLoc(x,y,multi.x_size);
      if(multi.colors[loc] == P_BLACK)
        inverted.colors[loc] = P_WHITE;
      else if(multi.colors[loc] == P_WHITE)
        inverted.colors[loc] = P_BLACK;
    }
  }
  inverted.regenChainsFromColors();
  BoardHistory invertedHist(inverted,P_BLACK,Rules::getTrompTaylorish(),0,false);
  vector<GoPattern::Pattern> invertedPatterns =
    GoPattern::detectEyes(inverted,invertedHist);
  const GoPattern::Pattern* invertedPattern = findEyeAt(
    invertedPatterns,
    multiCenter
  );
  testAssert(invertedPattern != nullptr);
  testAssert(invertedPattern->subject.color == P_WHITE);
  const GoPattern::EyeDetails& invertedDetails =
    get<GoPattern::EyeDetails>(invertedPattern->details);
  testAssert(invertedDetails.attacker == P_BLACK);
  testAssert(invertedDetails.capturedChains[0].id == "chain:W:B2");

  Board corner = Board::parseBoard(2,2,R"%%(
xx
x.
)%%");
  const Loc cornerPoint = Location::ofString("B1",corner);
  BoardHistory cornerHist(corner,P_WHITE,Rules::getSimpleTerritory(),0,false);
  vector<GoPattern::Pattern> cornerPatterns = GoPattern::detectEyes(corner,cornerHist);
  const GoPattern::Pattern* cornerPattern = findEyeAt(
    cornerPatterns,
    cornerPoint
  );
  testAssert(cornerPattern != nullptr);
  testAssert(cornerPattern->subject.id == "region:B:B1");

  Board edge = Board::parseBoard(3,3,R"%%(
.o.
oxo
x.x
)%%");
  const Loc edgePoint = Location::ofString("B1",edge);
  BoardHistory edgeHist(edge,P_WHITE,Rules::getSimpleTerritory(),0,false);
  vector<GoPattern::Pattern> edgePatterns = GoPattern::detectEyes(edge,edgeHist);
  const GoPattern::Pattern* edgePattern = findEyeAt(
    edgePatterns,
    edgePoint
  );
  testAssert(edgePattern != nullptr);
  testAssert(get<GoPattern::EyeDetails>(edgePattern->details).capturedChains.size() == 3);

  Board trueEye = Board::parseBoard(5,5,R"%%(
.....
..x..
.x.x.
..x..
.....
)%%");
  const Loc trueEyeCenter = Location::ofString("C3",trueEye);
  Rules suicideAllowed = Rules::getTrompTaylorish();
  Rules suicideForbidden = suicideAllowed;
  suicideForbidden.multiStoneSuicideLegal = false;
  BoardHistory allowedHist(trueEye,P_WHITE,suicideAllowed,0,false);
  BoardHistory forbiddenHist(trueEye,P_WHITE,suicideForbidden,0,false);
  testAssert(!allowedHist.isLegal(trueEye,trueEyeCenter,P_WHITE));
  testAssert(!forbiddenHist.isLegal(trueEye,trueEyeCenter,P_WHITE));
  testAssert(GoPattern::detectEyes(trueEye,allowedHist).empty());
  testAssert(GoPattern::detectEyes(trueEye,forbiddenHist).empty());

  BoardHistory positiveSuicideForbidden(board,P_WHITE,suicideForbidden,0,false);
  testAssert(positiveSuicideForbidden.isLegal(board,center,P_WHITE));
  testAssert(
    findEyeAt(GoPattern::detectEyes(board,positiveSuicideForbidden),center) != nullptr
  );

  Board falseNoCapture = Board::parseBoard(5,5,R"%%(
.....
.oxo.
.x.x.
..x..
.....
)%%");
  BoardHistory falseHist(falseNoCapture,P_WHITE,suicideAllowed,0,false);
  testAssert(GoPattern::detectEyes(falseNoCapture,falseHist).empty());
}

// RawEyeChain은 KataGo Board chain을 사용하지 않는 독립 raw-grid 연결요소다.
struct RawEyeChain {
  int color;
  vector<int> points;
  vector<int> liberties;
};

// rawEyeNeighbors는 행 우선 raw-grid 좌표의 판 안 직교 이웃을 반환한다.
// point와 width/height는 raw 배열 좌표계이며, 반환값은 결정적 좌표 목록이다.
static vector<int> rawEyeNeighbors(int point, int width, int height) {
  const int x = point % width;
  const int y = point / width;
  vector<int> neighbors;
  if(x > 0)
    neighbors.push_back(point-1);
  if(x+1 < width)
    neighbors.push_back(point+1);
  if(y > 0)
    neighbors.push_back(point-width);
  if(y+1 < height)
    neighbors.push_back(point+width);
  return neighbors;
}

// rawEyeChains는 0/1/2 raw 배열을 flood-fill해 돌 chain과 활로를 독립 계산한다.
// labels는 빈칸/흑/백 배열이며, 반환값은 Board 내부 chain 자료구조와 무관한 연결요소다.
static vector<RawEyeChain> rawEyeChains(
  const vector<int>& labels,
  int width,
  int height
) {
  vector<bool> visited(labels.size(),false);
  vector<RawEyeChain> chains;
  for(int initial = 0; initial < static_cast<int>(labels.size()); initial++) {
    if(labels[initial] == 0 || visited[initial])
      continue;
    RawEyeChain chain;
    chain.color = labels[initial];
    vector<int> queue{initial};
    visited[initial] = true;
    set<int> libertySet;
    for(size_t index = 0; index < queue.size(); index++) {
      const int point = queue[index];
      chain.points.push_back(point);
      for(int adjacent: rawEyeNeighbors(point,width,height)) {
        if(labels[adjacent] == 0)
          libertySet.insert(adjacent);
        else if(labels[adjacent] == chain.color && !visited[adjacent]) {
          visited[adjacent] = true;
          queue.push_back(adjacent);
        }
      }
    }
    sort(chain.points.begin(),chain.points.end());
    chain.liberties.assign(libertySet.begin(),libertySet.end());
    chains.push_back(std::move(chain));
  }
  return chains;
}

// rawGtpPoint는 작은 raw-grid 좌표를 KataGo와 같은 GTP 점 문자열로 독립 변환한다.
// point와 width/height는 행 우선 좌표계이며, 반환값은 I 열을 건너뛴 공개 좌표다.
static string rawGtpPoint(int point, int width, int height) {
  const int x = point % width;
  const int y = point / width;
  char column = static_cast<char>('A' + x + (x >= 8 ? 1 : 0));
  return string(1,column) + Global::intToString(height-y);
}

// RawEyePattern은 독립 판정기가 만든 공개 비교 필드다.
struct RawEyePattern {
  int owner;
  int attacker;
  int point;
  vector<vector<int>> capturedChains;
};

static string joinEyeStrings(const vector<string>& values, const string& delimiter);

// rawEyePattern은 raw 배열에 한 수를 적용해 실제 제거된 인접 chain이 있는지 독립 판정한다.
// labels는 원본 상태, attacker는 현재 차례 1/2이며, 반환값은 계약 후보가 아니면 false다.
static bool rawEyePattern(
  const vector<int>& labels,
  int width,
  int height,
  int point,
  int attacker,
  RawEyePattern& result
) {
  if(labels[point] != 0)
    return false;
  const int owner = 3-attacker;
  const vector<int> adjacentPoints = rawEyeNeighbors(point,width,height);
  if(adjacentPoints.empty())
    return false;
  for(int adjacent: adjacentPoints) {
    if(labels[adjacent] != owner)
      return false;
  }

  vector<RawEyeChain> beforeChains = rawEyeChains(labels,width,height);
  vector<vector<int>> captured;
  vector<int> after = labels;
  after[point] = attacker;
  for(const RawEyeChain& chain: beforeChains) {
    if(chain.color != owner)
      continue;
    bool touchesPoint = false;
    for(int adjacent: adjacentPoints) {
      if(find(chain.points.begin(),chain.points.end(),adjacent) != chain.points.end()) {
        touchesPoint = true;
        break;
      }
    }
    if(!touchesPoint || chain.liberties != vector<int>{point})
      continue;
    captured.push_back(chain.points);
    for(int capturedPoint: chain.points)
      after[capturedPoint] = 0;
  }
  if(captured.empty())
    return false;

  bool playedChainHasLiberty = false;
  for(const RawEyeChain& chain: rawEyeChains(after,width,height)) {
    if(
      chain.color == attacker &&
      find(chain.points.begin(),chain.points.end(),point) != chain.points.end() &&
      !chain.liberties.empty()
    ) {
      playedChainHasLiberty = true;
      break;
    }
  }
  if(!playedChainHasLiberty)
    return false;

  sort(captured.begin(),captured.end());
  result = RawEyePattern{owner,attacker,point,std::move(captured)};
  return true;
}

// rawEyePatternKey는 독립 후보의 봉투·chain ID·전체 좌표를 결정적 비교 문자열로 만든다.
// candidate와 width/height는 raw 판정 결과이며, 반환값은 제품 결과와 직접 비교할 키다.
static string rawEyePatternKey(const RawEyePattern& candidate, int width, int height) {
  const string owner = candidate.owner == 1 ? "B" : "W";
  const string attacker = candidate.attacker == 1 ? "B" : "W";
  const string point = rawGtpPoint(candidate.point,width,height);
  ostringstream out;
  out << "eye|1|region|" << owner << "|region:" << owner << ":" << point
      << "|" << point << "|opponentCanCaptureAdjacentChain|proven|" << attacker;
  vector<string> chainKeys;
  for(const vector<int>& chain: candidate.capturedChains) {
    vector<string> points;
    for(int rawPoint: chain)
      points.push_back(rawGtpPoint(rawPoint,width,height));
    sort(points.begin(),points.end());
    chainKeys.push_back("chain:" + owner + ":" + points[0] + "=" + joinEyeStrings(points,","));
  }
  sort(chainKeys.begin(),chainKeys.end());
  out << "|" << joinEyeStrings(chainKeys,";") << "|immediateLegalCapture";
  return out.str();
}

// productEyePatternKey는 제품 Eye typed 결과를 독립 키와 같은 형식으로 정규화한다.
// pattern과 board는 제품 결과이며, 반환값은 모든 공개 불변식을 포함한 비교 키다.
static string productEyePatternKey(const GoPattern::Pattern& pattern, const Board& board) {
  testAssert(pattern.kind == GoPattern::Kind::EYE);
  testAssert(pattern.schemaVersion == 1);
  testAssert(pattern.subject.type == GoPattern::SubjectType::REGION);
  testAssert(pattern.subject.points.size() == 1);
  testAssert(pattern.outcome == GoPattern::Outcome::OPPONENT_CAN_CAPTURE_ADJACENT_CHAIN);
  testAssert(pattern.confidence == GoPattern::Confidence::PROVEN);
  const string owner = pattern.subject.color == P_BLACK ? "B" : "W";
  const string point = Location::toString(pattern.subject.points[0],board);
  const GoPattern::EyeDetails& details = get<GoPattern::EyeDetails>(pattern.details);
  const string attacker = details.attacker == P_BLACK ? "B" : "W";
  testAssert(details.attacker == getOpp(pattern.subject.color));
  testAssert(!details.capturedChains.empty());
  vector<string> chainKeys;
  for(const GoPattern::EyeCapturedChain& chain: details.capturedChains) {
    vector<string> points;
    for(Loc loc: chain.points)
      points.push_back(Location::toString(loc,board));
    sort(points.begin(),points.end());
    points.erase(unique(points.begin(),points.end()),points.end());
    chainKeys.push_back(chain.id + "=" + joinEyeStrings(points,","));
  }
  sort(chainKeys.begin(),chainKeys.end());
  ostringstream out;
  out << "eye|1|region|" << owner << "|" << pattern.subject.id
      << "|" << point << "|opponentCanCaptureAdjacentChain|proven|" << attacker
      << "|" << joinEyeStrings(chainKeys,";") << "|immediateLegalCapture";
  return out.str();
}

// sha256String은 고정된 열거 자료를 소문자 SHA-256 문자열로 바꾼다.
// input은 원문 자료이며, 반환값은 64자리 digest다.
static string sha256String(const string& input) {
  char hash[65];
  SHA2::get256(reinterpret_cast<const uint8_t*>(input.data()),input.size(),hash);
  return string(hash);
}

// joinEyeStrings는 결정적 검증 문자열 목록을 지정 구분자로 연결한다.
// values는 입력 목록, delimiter는 구분자이며, 반환값은 빈 목록이면 빈 문자열이다.
static string joinEyeStrings(const vector<string>& values, const string& delimiter) {
  ostringstream out;
  for(size_t index = 0; index < values.size(); index++) {
    if(index > 0)
      out << delimiter;
    out << values[index];
  }
  return out.str();
}

// runEyeIndependentExhaustiveTest는 2×2~3×3 전체 합법 raw 상태를 제품과 독립 전수 대조한다.
// 반환값은 없으며 상태·후보 수, fingerprint, 제품/오라클 digest를 고정한다.
static void runEyeIndependentExhaustiveTest() {
  struct ExpectedCount {
    int width;
    int height;
    int states;
    int candidates;
  };
  const vector<ExpectedCount> expectedCounts = {
    {2,2,57,8},
    {2,3,489,44},
    {3,2,489,44},
    {3,3,12675,1306},
  };
  ostringstream stateStream;
  ostringstream oracleStream;
  ostringstream productStream;
  stateStream << "seed=ternary-little-endian-v1\n";
  oracleStream << "seed=ternary-little-endian-v1\n";
  productStream << "seed=ternary-little-endian-v1\n";

  int totalStates = 0;
  int totalCandidates = 0;
  for(const ExpectedCount& expected: expectedCounts) {
    int stateLimit = 1;
    for(int point = 0; point < expected.width * expected.height; point++)
      stateLimit *= 3;
    int comparedStates = 0;
    int comparedCandidates = 0;
    for(int state = 0; state < stateLimit; state++) {
      vector<int> labels(expected.width*expected.height,0);
      int remaining = state;
      for(int point = 0; point < static_cast<int>(labels.size()); point++) {
        labels[point] = remaining % 3;
        remaining /= 3;
      }
      const vector<RawEyeChain> rawChains =
        rawEyeChains(labels,expected.width,expected.height);
      bool zeroLiberty = false;
      for(const RawEyeChain& chain: rawChains) {
        if(chain.liberties.empty()) {
          zeroLiberty = true;
          break;
        }
      }
      if(zeroLiberty)
        continue;

      comparedStates += 1;
      stateStream << expected.width << "x" << expected.height << "|" << state << "|";
      for(int label: labels)
        stateStream << label;
      stateStream << "\n";

      Board board(expected.width,expected.height);
      for(int point = 0; point < static_cast<int>(labels.size()); point++) {
        const int x = point % expected.width;
        const int y = point / expected.width;
        Loc loc = Location::getLoc(x,y,expected.width);
        board.colors[loc] = labels[point] == 0 ? C_EMPTY :
          (labels[point] == 1 ? P_BLACK : P_WHITE);
      }
      board.regenChainsFromColors();
      Rules rules = Rules::getSimpleTerritory();

      for(int attacker: {1,2}) {
        vector<string> expectedKeys;
        for(int point = 0; point < static_cast<int>(labels.size()); point++) {
          RawEyePattern candidate;
          if(rawEyePattern(
            labels,
            expected.width,
            expected.height,
            point,
            attacker,
            candidate
          ))
            expectedKeys.push_back(
              rawEyePatternKey(candidate,expected.width,expected.height)
            );
        }
        sort(expectedKeys.begin(),expectedKeys.end());

        const Player nextPlayer = attacker == 1 ? P_BLACK : P_WHITE;
        BoardHistory hist(board,nextPlayer,rules,0,false);
        vector<GoPattern::Pattern> actual = GoPattern::detectEyes(board,hist);
        vector<string> actualKeys;
        for(const GoPattern::Pattern& pattern: actual)
          actualKeys.push_back(productEyePatternKey(pattern,board));
        sort(actualKeys.begin(),actualKeys.end());
        if(actualKeys != expectedKeys) {
          cerr << "Eye exhaustive mismatch " << expected.width << "x" << expected.height
               << " state=" << state << " attacker=" << attacker << endl;
          cerr << "expected=" << joinEyeStrings(expectedKeys,"#") << endl;
          cerr << "actual=" << joinEyeStrings(actualKeys,"#") << endl;
        }
        testAssert(actualKeys == expectedKeys);

        comparedCandidates += static_cast<int>(expectedKeys.size());
        const string prefix = Global::strprintf(
          "%dx%d|%d|%d|",
          expected.width,
          expected.height,
          state,
          attacker
        );
        oracleStream << prefix << joinEyeStrings(expectedKeys,"#") << "\n";
        productStream << prefix << joinEyeStrings(actualKeys,"#") << "\n";
      }
    }
    testAssert(comparedStates == expected.states);
    testAssert(comparedCandidates == expected.candidates);
    cout << "Eye independent " << expected.width << "x" << expected.height
         << ": " << comparedStates << " states, " << comparedCandidates
         << " candidates" << endl;
    totalStates += comparedStates;
    totalCandidates += comparedCandidates;
  }

  const string stateFingerprint = sha256String(stateStream.str());
  const string oracleDigest = sha256String(oracleStream.str());
  const string productDigest = sha256String(productStream.str());
  cout
    << "Eye independent validation: " << totalStates << " states, "
    << totalCandidates << " candidates, 0 mismatches, state fingerprint "
    << stateFingerprint << ", oracle digest " << oracleDigest
    << ", product digest " << productDigest << endl;
  testAssert(totalStates == 13710);
  testAssert(totalCandidates == 1402);
  testAssert(
    stateFingerprint == "31a09396d3f0a9280dfd97e8c1b72c4c5a7fc3c8c26a5918c5654c02d71944b2"
  );
  testAssert(
    oracleDigest == "4d917a709b5d1d096999f4d22df7fb25aa87ddb36f3826bf055f80408e21563e"
  );
  testAssert(productDigest == oracleDigest);
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

  int orientationCount = 0;
  int emitted = 0;
  for(const vector<pair<int,int>>& shape: shapes) {
    set<vector<pair<int,int>>> orientations;
    for(int symmetry = 0; symmetry < 8; symmetry++)
      orientations.insert(transformNakadeTestShape(shape,symmetry));
    for(const vector<pair<int,int>>& orientation: orientations) {
      vector<pair<int,int>> emptyPoints = translatedNakadePoints(orientation,3,3);
      Board board = makeNakadeFixtureBoard(12,12,emptyPoints);
      BoardHistory hist(board,P_WHITE,Rules::getTrompTaylorish(),0,false);
      GoPattern::DetectionResult result = GoPattern::detect(board,hist);
      const bool actual = findKind(result.patterns,GoPattern::Kind::NAKADE) != nullptr;
      const bool expected = supported.find(shape) != supported.end();
      testAssert(actual == expected);
      orientationCount++;
      if(actual)
        emitted++;
    }
  }
  cout << "Nakade catalog validation: " << shapes.size()
       << " free polyominoes, " << orientationCount
       << " fixed orientations, " << emitted << " supported orientations" << endl;
  testAssert(orientationCount == 304);
  testAssert(emitted == 23);
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

// runSemeaiPatternTest는 좁은 sharedLibertyRace 봉투와 제외 경계를 검증한다.
static void runSemeaiPatternTest() {
  Board board = Board::parseBoard(3,3,R"%%(
...
.o.
.xx
)%%");
  BoardHistory blackTurn(board,P_BLACK,Rules::getTrompTaylorish(),0,false);
  vector<GoPattern::Pattern> patterns = GoPattern::detectSemeai(board,blackTurn);
  testAssert(patterns.size() == 1);
  const GoPattern::Pattern& pattern = patterns[0];
  testAssert(pattern.kind == GoPattern::Kind::SEMEAI);
  testAssert(pattern.schemaVersion == 1);
  testAssert(pattern.subject.type == GoPattern::SubjectType::REGION);
  testAssert(pattern.subject.color == C_EMPTY);
  testAssert(pattern.subject.id == "region:E:C2");
  testAssert(pattern.subject.points.size() == 1);
  testAssert(pattern.outcome == GoPattern::Outcome::UNSETTLED);
  testAssert(pattern.confidence == GoPattern::Confidence::MEDIUM);

  const GoPattern::SemeaiDetails& details =
    get<GoPattern::SemeaiDetails>(pattern.details);
  testAssert(details.currentPlayer == P_BLACK);
  testAssert(details.currentPlayerSubjectId == "chain:B:B1");
  testAssert(details.opponentSubjectId == "chain:W:B2");
  testAssert(details.currentPlayerOutsideLibertyCount == 1);
  testAssert(details.opponentOutsideLibertyCount == 2);
  testAssert(details.sharedLibertyCount == 1);

  json encoded = GoPattern::toJson(pattern,board);
  testAssert(encoded["kind"] == "semeai");
  testAssert(encoded["schemaVersion"] == 1);
  testAssert(encoded["subject"]["type"] == "region");
  testAssert(encoded["subject"]["color"] == "E");
  testAssert(encoded["subject"]["id"] == "region:E:C2");
  testAssert(encoded["subject"]["points"] == json::array({"C2"}));
  testAssert(encoded["outcome"] == "unsettled");
  testAssert(encoded["confidence"] == "medium");
  testAssert(encoded["details"]["currentPlayer"] == "B");
  testAssert(encoded["details"]["currentPlayerSubjectId"] == "chain:B:B1");
  testAssert(encoded["details"]["opponentSubjectId"] == "chain:W:B2");
  testAssert(encoded["details"]["currentPlayerOutsideLibertyCount"] == 1);
  testAssert(encoded["details"]["opponentOutsideLibertyCount"] == 2);
  testAssert(encoded["details"]["sharedLibertyCount"] == 1);
  testAssert(encoded["details"]["semantics"] == "sharedLibertyRace");

  BoardHistory whiteTurn(board,P_WHITE,Rules::getTrompTaylorish(),0,false);
  vector<GoPattern::Pattern> whitePatterns = GoPattern::detectSemeai(board,whiteTurn);
  testAssert(whitePatterns.size() == 1);
  const GoPattern::SemeaiDetails& whiteDetails =
    get<GoPattern::SemeaiDetails>(whitePatterns[0].details);
  testAssert(whiteDetails.currentPlayer == P_WHITE);
  testAssert(whiteDetails.currentPlayerSubjectId == "chain:W:B2");
  testAssert(whiteDetails.opponentSubjectId == "chain:B:B1");
  testAssert(whiteDetails.currentPlayerOutsideLibertyCount == 2);
  testAssert(whiteDetails.opponentOutsideLibertyCount == 1);

  GoPattern::DetectionResult publicResult = GoPattern::detect(board,blackTurn);
  testAssert(findKind(publicResult.patterns,GoPattern::Kind::SEMEAI) != nullptr);

  Board disconnected = Board::parseBoard(4,4,R"%%(
....
..o.
.x..
....
)%%");
  BoardHistory disconnectedHist(
    disconnected,
    P_BLACK,
    Rules::getTrompTaylorish(),
    0,
    false
  );
  testAssert(GoPattern::detectSemeai(disconnected,disconnectedHist).empty());

  Board thirdChain = Board::parseBoard(3,3,R"%%(
x..
.o.
.xx
)%%");
  BoardHistory thirdChainHist(
    thirdChain,
    P_BLACK,
    Rules::getTrompTaylorish(),
    0,
    false
  );
  testAssert(GoPattern::detectSemeai(thirdChain,thirdChainHist).empty());
}

// OracleSemeaiChain은 Board chain 자료구조를 사용하지 않는 독립 격자 연결요소다.
struct OracleSemeaiChain {
  Player color;
  vector<int> points;
  set<int> liberties;
  string subjectId;
};

// OracleSemeaiPattern은 독립 구조 판정기의 공개 비교 필드다.
struct OracleSemeaiPattern {
  string subjectId;
  vector<string> sharedPoints;
  string currentPlayerSubjectId;
  string opponentSubjectId;
  int currentOutside;
  int opponentOutside;
};

// oracleSemeaiNeighbors는 선형 격자 좌표의 판 안 직교 이웃을 반환한다.
static vector<int> oracleSemeaiNeighbors(int point, int width, int height) {
  const int x = point % width;
  const int y = point / width;
  vector<int> neighbors;
  if(x > 0)
    neighbors.push_back(point-1);
  if(x+1 < width)
    neighbors.push_back(point+1);
  if(y > 0)
    neighbors.push_back(point-width);
  if(y+1 < height)
    neighbors.push_back(point+width);
  return neighbors;
}

// oracleSemeaiPointString은 독립 격자 좌표를 공개 GTP 좌표로 바꾼다.
static string oracleSemeaiPointString(int point, const Board& board) {
  const int x = point % board.x_size;
  const int y = point / board.x_size;
  return Location::toString(Location::getLoc(x,y,board.x_size),board);
}

// oracleSemeaiChains는 색 격자를 BFS해 chain·활로·결정적 ID를 독립 구성한다.
static vector<OracleSemeaiChain> oracleSemeaiChains(
  const Board& board,
  vector<int>& labels
) {
  const int pointCount = board.x_size*board.y_size;
  labels.assign(pointCount,-1);
  vector<OracleSemeaiChain> chains;
  for(int point = 0; point < pointCount; point++) {
    const int x = point % board.x_size;
    const int y = point / board.x_size;
    const Player color = board.colors[Location::getLoc(x,y,board.x_size)];
    if((color != P_BLACK && color != P_WHITE) || labels[point] >= 0)
      continue;

    const int label = static_cast<int>(chains.size());
    OracleSemeaiChain chain;
    chain.color = color;
    vector<int> queue;
    queue.push_back(point);
    labels[point] = label;
    for(size_t queueIndex = 0; queueIndex < queue.size(); queueIndex++) {
      const int current = queue[queueIndex];
      chain.points.push_back(current);
      for(int adjacent: oracleSemeaiNeighbors(current,board.x_size,board.y_size)) {
        const int adjacentX = adjacent % board.x_size;
        const int adjacentY = adjacent / board.x_size;
        const Player adjacentColor =
          board.colors[Location::getLoc(adjacentX,adjacentY,board.x_size)];
        if(adjacentColor == C_EMPTY)
          chain.liberties.insert(adjacent);
        else if(adjacentColor == color && labels[adjacent] < 0) {
          labels[adjacent] = label;
          queue.push_back(adjacent);
        }
      }
    }
    string anchor = oracleSemeaiPointString(chain.points[0],board);
    for(size_t index = 1; index < chain.points.size(); index++)
      anchor = min(anchor,oracleSemeaiPointString(chain.points[index],board));
    chain.subjectId =
      string("chain:") + (color == P_BLACK ? "B:" : "W:") + anchor;
    chains.push_back(std::move(chain));
  }
  return chains;
}

// oracleSemeaiSharedConnected는 공유 활로가 하나의 직교 연결요소인지 검사한다.
static bool oracleSemeaiSharedConnected(
  const set<int>& shared,
  int width,
  int height
) {
  if(shared.empty())
    return false;
  set<int> reached;
  vector<int> queue;
  queue.push_back(*shared.begin());
  reached.insert(*shared.begin());
  for(size_t index = 0; index < queue.size(); index++) {
    for(int adjacent: oracleSemeaiNeighbors(queue[index],width,height)) {
      if(shared.find(adjacent) != shared.end() && reached.insert(adjacent).second)
        queue.push_back(adjacent);
    }
  }
  return reached.size() == shared.size();
}

// oracleSemeaiPairIsolated는 참가 돌·전체 활로 주변에 제3 chain이 없는지 격자 label로 검사한다.
static bool oracleSemeaiPairIsolated(
  const OracleSemeaiChain& left,
  const OracleSemeaiChain& right,
  const vector<int>& labels,
  int leftLabel,
  int rightLabel,
  int width,
  int height
) {
  vector<int> inspected = left.points;
  inspected.insert(inspected.end(),right.points.begin(),right.points.end());
  inspected.insert(inspected.end(),left.liberties.begin(),left.liberties.end());
  inspected.insert(inspected.end(),right.liberties.begin(),right.liberties.end());
  for(int point: inspected) {
    for(int adjacent: oracleSemeaiNeighbors(point,width,height)) {
      const int label = labels[adjacent];
      if(label >= 0 && label != leftLabel && label != rightLabel)
        return false;
    }
  }
  return true;
}

// oracleSemeaiPatterns는 독립 격자 계산으로 v1 후보와 공개 필드를 만든다.
static vector<OracleSemeaiPattern> oracleSemeaiPatterns(
  const Board& board,
  Player nextPlayer
) {
  vector<int> labels;
  vector<OracleSemeaiChain> chains = oracleSemeaiChains(board,labels);
  vector<OracleSemeaiPattern> patterns;
  for(size_t leftIndex = 0; leftIndex < chains.size(); leftIndex++) {
    for(size_t rightIndex = leftIndex+1; rightIndex < chains.size(); rightIndex++) {
      const OracleSemeaiChain& left = chains[leftIndex];
      const OracleSemeaiChain& right = chains[rightIndex];
      if(left.color == right.color)
        continue;
      if(
        left.liberties.size() < 2 || left.liberties.size() > 4 ||
        right.liberties.size() < 2 || right.liberties.size() > 4
      )
        continue;

      set<int> shared;
      set_intersection(
        left.liberties.begin(),
        left.liberties.end(),
        right.liberties.begin(),
        right.liberties.end(),
        inserter(shared,shared.end())
      );
      if(!oracleSemeaiSharedConnected(shared,board.x_size,board.y_size))
        continue;
      if(!oracleSemeaiPairIsolated(
        left,
        right,
        labels,
        static_cast<int>(leftIndex),
        static_cast<int>(rightIndex),
        board.x_size,
        board.y_size
      ))
        continue;

      const OracleSemeaiChain& current =
        left.color == nextPlayer ? left : right;
      const OracleSemeaiChain& opponent =
        left.color == nextPlayer ? right : left;
      vector<string> sharedPoints;
      for(int point: shared)
        sharedPoints.push_back(oracleSemeaiPointString(point,board));
      sort(sharedPoints.begin(),sharedPoints.end());
      patterns.push_back(OracleSemeaiPattern{
        "region:E:" + sharedPoints[0],
        std::move(sharedPoints),
        current.subjectId,
        opponent.subjectId,
        static_cast<int>(current.liberties.size()-shared.size()),
        static_cast<int>(opponent.liberties.size()-shared.size()),
      });
    }
  }
  return patterns;
}

// semeaiPatternKey는 제품·독립 판정기의 공개 필드를 같은 결정적 문자열로 바꾼다.
static string semeaiPatternKey(
  const string& subjectId,
  const vector<string>& sharedPoints,
  const string& currentSubjectId,
  const string& opponentSubjectId,
  int currentOutside,
  int opponentOutside
) {
  string key = subjectId;
  for(const string& point: sharedPoints)
    key += "|" + point;
  key += "|" + currentSubjectId;
  key += "|" + opponentSubjectId;
  key += "|" + to_string(currentOutside);
  key += "|" + to_string(opponentOutside);
  key += "|" + to_string(sharedPoints.size());
  return key;
}

// runSemeaiIndependentPropertyTest는 2×2~3×3 정사각·비정사각 전체 상태를 독립 대조한다.
static void runSemeaiIndependentPropertyTest() {
  int comparedCandidates = 0;
  int comparedStates = 0;
  const vector<pair<int,int>> sizes = {{2,2},{2,3},{3,2},{3,3}};
  for(const pair<int,int>& size: sizes) {
    int stateCount = 1;
    for(int point = 0; point < size.first*size.second; point++)
      stateCount *= 3;
    for(int state = 0; state < stateCount; state++) {
      Board board(size.first,size.second);
      int remaining = state;
      for(int y = 0; y < board.y_size; y++) {
        for(int x = 0; x < board.x_size; x++) {
          const int value = remaining % 3;
          remaining /= 3;
          Loc loc = Location::getLoc(x,y,board.x_size);
          board.colors[loc] =
            value == 0 ? C_EMPTY : (value == 1 ? P_BLACK : P_WHITE);
        }
      }
      board.regenChainsFromColors();

      bool invalid = false;
      for(int y = 0; y < board.y_size && !invalid; y++) {
        for(int x = 0; x < board.x_size; x++) {
          Loc loc = Location::getLoc(x,y,board.x_size);
          if(
            (board.colors[loc] == P_BLACK || board.colors[loc] == P_WHITE) &&
            board.getNumLiberties(loc) == 0
          ) {
            invalid = true;
            break;
          }
        }
      }
      if(invalid)
        continue;

      for(Player nextPlayer: {P_BLACK,P_WHITE}) {
        vector<OracleSemeaiPattern> expected =
          oracleSemeaiPatterns(board,nextPlayer);
        BoardHistory hist(board,nextPlayer,Rules::getTrompTaylorish(),0,false);
        vector<GoPattern::Pattern> actual = GoPattern::detectSemeai(board,hist);
        set<string> expectedKeys;
        for(const OracleSemeaiPattern& candidate: expected) {
          expectedKeys.insert(semeaiPatternKey(
            candidate.subjectId,
            candidate.sharedPoints,
            candidate.currentPlayerSubjectId,
            candidate.opponentSubjectId,
            candidate.currentOutside,
            candidate.opponentOutside
          ));
        }
        set<string> actualKeys;
        for(const GoPattern::Pattern& candidate: actual) {
          testAssert(candidate.kind == GoPattern::Kind::SEMEAI);
          testAssert(candidate.subject.color == C_EMPTY);
          testAssert(candidate.outcome == GoPattern::Outcome::UNSETTLED);
          testAssert(candidate.confidence == GoPattern::Confidence::MEDIUM);
          vector<string> sharedPoints;
          for(Loc point: candidate.subject.points)
            sharedPoints.push_back(Location::toString(point,board));
          sort(sharedPoints.begin(),sharedPoints.end());
          const GoPattern::SemeaiDetails& details =
            get<GoPattern::SemeaiDetails>(candidate.details);
          testAssert(details.currentPlayer == nextPlayer);
          actualKeys.insert(semeaiPatternKey(
            candidate.subject.id,
            sharedPoints,
            details.currentPlayerSubjectId,
            details.opponentSubjectId,
            details.currentPlayerOutsideLibertyCount,
            details.opponentOutsideLibertyCount
          ));
        }
        testAssert(actualKeys == expectedKeys);
        if(nextPlayer == P_BLACK) {
          comparedCandidates += static_cast<int>(expected.size());
          comparedStates++;
        }
      }
    }
  }
  testAssert(comparedCandidates >= 500);
  cout << "Semeai structural validation: " << comparedStates
       << " states, " << comparedCandidates << " candidates, 0 mismatches" << endl;
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

// makeDenseEyePositiveBoard는 19×19에 간격을 둔 즉시 포획 눈 9개를 배치한다.
// 반환값은 각 5×5 tile의 흰 외곽·검은 내부 ring·빈 중심이 보존된 합법 보드다.
static Board makeDenseEyePositiveBoard() {
  Board board(19,19);
  for(int baseY: {0,6,12}) {
    for(int baseX: {0,6,12}) {
      for(int localY = 0; localY < 5; localY++) {
        for(int localX = 0; localX < 5; localX++) {
          Loc loc = Location::getLoc(baseX+localX,baseY+localY,board.x_size);
          const bool boundary =
            localX == 0 || localX == 4 || localY == 0 || localY == 4;
          if(boundary)
            board.colors[loc] = P_WHITE;
          else if(localX == 2 && localY == 2)
            board.colors[loc] = C_EMPTY;
          else
            board.colors[loc] = P_BLACK;
        }
      }
    }
  }
  board.regenChainsFromColors();
  for(int y = 0; y < board.y_size; y++) {
    for(int x = 0; x < board.x_size; x++) {
      Loc loc = Location::getLoc(x,y,board.x_size);
      if(board.colors[loc] == P_BLACK || board.colors[loc] == P_WHITE)
        testAssert(board.getNumLiberties(loc) > 0);
    }
  }
  return board;
}

// runEyePositivePerformanceTest는 Eye 양성 9개가 있는 19×19 full detect의 p95 예산을 검증한다.
// 반환값은 없으며 실제 BoardHistory replay 증명과 200회 중 95백분위 20ms 상한을 함께 검사한다.
static void runEyePositivePerformanceTest() {
  Board board = makeDenseEyePositiveBoard();
  BoardHistory hist(board,P_WHITE,Rules::getTrompTaylorish(),0,false);
  const Hash128 originalHash = board.pos_hash;
  const size_t originalHistorySize = hist.moveHistory.size();

  GoPattern::DetectionResult initial = GoPattern::detect(board,hist);
  int eyeCount = 0;
  int replayedEyeCount = 0;
  for(const GoPattern::Pattern& pattern: initial.patterns) {
    if(pattern.kind != GoPattern::Kind::EYE)
      continue;
    eyeCount += 1;
    const GoPattern::EyeDetails& details = get<GoPattern::EyeDetails>(pattern.details);
    testAssert(pattern.subject.points.size() == 1);
    const Loc move = pattern.subject.points[0];
    testAssert(details.attacker == P_WHITE);
    testAssert(hist.isLegal(board,move,details.attacker));
    testAssert(!hist.isPassForKo(board,move,details.attacker));
    Board replayBoard(board);
    BoardHistory replayHist(hist);
    replayHist.makeBoardMoveAssumeLegal(replayBoard,move,details.attacker,NULL);
    for(const GoPattern::EyeCapturedChain& captured: details.capturedChains) {
      for(Loc point: captured.points)
        testAssert(replayBoard.colors[point] == C_EMPTY);
    }
    replayedEyeCount += 1;
  }
  testAssert(eyeCount >= 9);
  testAssert(replayedEyeCount == eyeCount);
  testAssert(board.pos_hash == originalHash);
  testAssert(hist.moveHistory.size() == originalHistorySize);

  vector<double> milliseconds;
  milliseconds.reserve(200);
  for(int iteration = 0; iteration < 200; iteration++) {
    auto start = chrono::steady_clock::now();
    GoPattern::DetectionResult result = GoPattern::detect(board,hist);
    auto end = chrono::steady_clock::now();
    testAssert(!result.truncated);
    int detectedEyes = 0;
    for(const GoPattern::Pattern& pattern: result.patterns) {
      if(pattern.kind == GoPattern::Kind::EYE)
        detectedEyes += 1;
    }
    testAssert(detectedEyes >= 9);
    milliseconds.push_back(chrono::duration<double,std::milli>(end-start).count());
  }
  sort(milliseconds.begin(),milliseconds.end());
  const double p95 = milliseconds[189];
  cout << "Eye-positive 19x19 full Pattern postprocess p95: "
       << p95 << " ms (" << eyeCount << " eyes replayed)" << endl;
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
  runEyePatternTest();
  runEyeHistoryBoundaryTest();
  runEyeSnapbackFixtureTest();
  runEyeIndependentExhaustiveTest();
  runNakadePatternTest();
  runNakadeCatalogExhaustiveTest();
  runSemeaiPatternTest();
  runSemeaiIndependentPropertyTest();
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
  runEyePositivePerformanceTest();
}
