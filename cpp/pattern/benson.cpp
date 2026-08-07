#include "benson.h"

#include <algorithm>
#include <cassert>
#include <iterator>
#include <set>
#include <stdexcept>

using namespace std;
namespace GoPattern {
namespace {

// BensonRegion은 한 색 기준의 최대 empty-or-opponent 영역과 활력 관계를 보존한다.
struct BensonRegion {
  vector<Loc> points;
  vector<Loc> borderingChainHeads;
  vector<Loc> vitalForChainHeads;
  string id;
};

// appendUnique는 좌표 목록에 아직 없는 좌표만 추가한다.
// values는 수정할 목록이고 value는 추가 후보이며, 반환값은 없다.
static void appendUnique(vector<Loc>& values, Loc value) {
  if(find(values.begin(),values.end(),value) == values.end())
    values.push_back(value);
}

// isAdjacentToChain은 한 점이 지정 chain의 돌과 상하좌우로 맞닿는지 검사한다.
// board는 보드, loc은 검사할 점, pla는 chain 색, chainHead는 대상 chain 대표 좌표다.
// 반환값은 인접하면 true다.
static bool isAdjacentToChain(const Board& board, Loc loc, Player pla, Loc chainHead) {
  for(int i = 0; i < 4; i++) {
    Loc adj = loc + board.adj_offsets[i];
    if(board.colors[adj] == pla && board.chain_head[adj] == chainHead)
      return true;
  }
  return false;
}

// chainPoints는 chain의 원형 연결 목록을 완전한 좌표 목록으로 펼친다.
// board는 보드이고 chainHead는 대상 chain 대표 좌표다.
// 반환값은 chain에 속한 모든 돌 좌표다.
static vector<Loc> chainPoints(const Board& board, Loc chainHead) {
  vector<Loc> points;
  Loc loc = chainHead;
  do {
    points.push_back(loc);
    loc = board.next_in_chain[loc];
  } while(loc != chainHead);
  return points;
}

// stablePointID는 좌표 집합의 사전순 최소 GTP 좌표로 결정적 ID를 만든다.
// prefix는 type과 color를 포함한 접두사, points는 비어 있지 않은 좌표 집합, board는 좌표계다.
// 반환값은 "prefix:좌표" 형식의 ID다.
static string stablePointID(const string& prefix, const vector<Loc>& points, const Board& board) {
  assert(!points.empty());
  string anchor = Location::toString(points[0],board);
  for(size_t i = 1; i < points.size(); i++)
    anchor = min(anchor,Location::toString(points[i],board));
  return prefix + ":" + anchor;
}

// buildRegions는 한 색의 Benson 계산에 필요한 최대 empty-or-opponent 영역을 만든다.
// board는 보드, pla는 생존을 판정할 색, suicideLegal은 활력 정의의 자살 허용 모드다.
// 반환값은 영역 좌표, 경계 chain, 활력 chain을 모두 담은 목록이다.
static vector<BensonRegion> buildRegions(const Board& board, Player pla, bool suicideLegal) {
  const Player opp = getOpp(pla);
  bool visited[Board::MAX_ARR_SIZE];
  fill(visited,visited+Board::MAX_ARR_SIZE,false);

  vector<BensonRegion> regions;
  Loc queue[Board::MAX_ARR_SIZE];
  for(int y = 0; y < board.y_size; y++) {
    for(int x = 0; x < board.x_size; x++) {
      Loc initialLoc = Location::getLoc(x,y,board.x_size);
      if(board.colors[initialLoc] != C_EMPTY || visited[initialLoc])
        continue;

      BensonRegion region;
      int queueHead = 0;
      int queueTail = 1;
      queue[0] = initialLoc;
      visited[initialLoc] = true;

      while(queueHead < queueTail) {
        Loc loc = queue[queueHead++];
        region.points.push_back(loc);
        for(int i = 0; i < 4; i++) {
          Loc adj = loc + board.adj_offsets[i];
          if(board.colors[adj] == pla)
            appendUnique(region.borderingChainHeads,board.chain_head[adj]);
          else if(
            (board.colors[adj] == C_EMPTY || board.colors[adj] == opp) &&
            !visited[adj]
          ) {
            visited[adj] = true;
            queue[queueTail++] = adj;
          }
        }
      }

      for(Loc chainHead: region.borderingChainHeads) {
        bool vital = true;
        for(Loc loc: region.points) {
          if((suicideLegal || board.colors[loc] == C_EMPTY) &&
             !isAdjacentToChain(board,loc,pla,chainHead)) {
            vital = false;
            break;
          }
        }
        if(vital)
          region.vitalForChainHeads.push_back(chainHead);
      }

      const string color = pla == P_BLACK ? "B" : "W";
      region.id = stablePointID("region:" + color,region.points,board);
      sort(region.borderingChainHeads.begin(),region.borderingChainHeads.end());
      sort(region.vitalForChainHeads.begin(),region.vitalForChainHeads.end());
      regions.push_back(std::move(region));
    }
  }
  return regions;
}

// survivingChains는 Benson의 최대 고정점을 계산한다.
// chainHeads는 해당 색의 모든 chain, regions는 활력·경계 관계가 계산된 영역이다.
// 반환값은 certificate에 포함될 pass-alive chain 대표 좌표 집합이다.
static set<Loc> survivingChains(const vector<Loc>& chainHeads, const vector<BensonRegion>& regions) {
  set<Loc> alive(chainHeads.begin(),chainHeads.end());
  while(true) {
    vector<Loc> toRemove;
    for(Loc chainHead: alive) {
      int vitalRegionCount = 0;
      for(const BensonRegion& region: regions) {
        bool regionActive = true;
        for(Loc borderingHead: region.borderingChainHeads) {
          if(alive.find(borderingHead) == alive.end()) {
            regionActive = false;
            break;
          }
        }
        if(
          regionActive &&
          find(region.vitalForChainHeads.begin(),region.vitalForChainHeads.end(),chainHead) !=
            region.vitalForChainHeads.end()
        )
          vitalRegionCount += 1;
      }
      if(vitalRegionCount < 2)
        toRemove.push_back(chainHead);
    }
    if(toRemove.empty())
      break;
    for(Loc chainHead: toRemove)
      alive.erase(chainHead);
  }
  return alive;
}

// verifyCertificate는 고정점 결과가 독립적인 국소 불변식과 KataGo pass-alive 결과를 모두 만족하는지 검사한다.
// board는 보드, pla는 대상 색, suicideLegal은 활력 모드, regions와 alive는 검증할 certificate다.
// 반환값은 모든 chain·영역 증명이 일치할 때 true다.
static bool verifyCertificate(
  const Board& board,
  Player pla,
  bool suicideLegal,
  const vector<BensonRegion>& regions,
  const set<Loc>& alive
) {
  Color area[Board::MAX_ARR_SIZE];
  board.calculateArea(area,false,false,false,suicideLegal);

  for(int y = 0; y < board.y_size; y++) {
    for(int x = 0; x < board.x_size; x++) {
      Loc loc = Location::getLoc(x,y,board.x_size);
      if(board.colors[loc] != pla)
        continue;
      bool certificateAlive = alive.find(board.chain_head[loc]) != alive.end();
      bool kataGoAlive = area[loc] == pla;
      if(certificateAlive != kataGoAlive)
        return false;
    }
  }

  for(Loc chainHead: alive) {
    int vitalRegionCount = 0;
    for(const BensonRegion& region: regions) {
      bool allBordersCertified = true;
      for(Loc borderingHead: region.borderingChainHeads) {
        if(alive.find(borderingHead) == alive.end()) {
          allBordersCertified = false;
          break;
        }
      }
      if(!allBordersCertified)
        continue;

      bool vital = true;
      for(Loc loc: region.points) {
        if((suicideLegal || board.colors[loc] == C_EMPTY) &&
           !isAdjacentToChain(board,loc,pla,chainHead)) {
          vital = false;
          break;
        }
      }
      if(vital)
        vitalRegionCount += 1;
    }
    if(vitalRegionCount < 2)
      return false;
  }
  return true;
}

// detectForPlayer는 한 색의 certificate를 만들고 proven BensonAlive 패턴으로 변환한다.
// board는 보드, pla는 대상 색, suicideLegal은 pass-alive 활력 모드다.
// 반환값은 검증을 통과한 해당 색의 패턴이며 certificate 불일치 시 빈 목록이다.
static vector<Pattern> detectForPlayer(const Board& board, Player pla, bool suicideLegal) {
  vector<Loc> chainHeads;
  for(int y = 0; y < board.y_size; y++) {
    for(int x = 0; x < board.x_size; x++) {
      Loc loc = Location::getLoc(x,y,board.x_size);
      if(board.colors[loc] == pla && board.chain_head[loc] == loc)
        chainHeads.push_back(loc);
    }
  }

  vector<BensonRegion> regions = buildRegions(board,pla,suicideLegal);
  set<Loc> alive = survivingChains(chainHeads,regions);
  if(!verifyCertificate(board,pla,suicideLegal,regions,alive))
    throw runtime_error("Benson certificate와 KataGo pass-alive 결과가 일치하지 않습니다");

  vector<Pattern> patterns;
  const string color = pla == P_BLACK ? "B" : "W";
  for(Loc chainHead: alive) {
    vector<BensonVitalRegion> vitalRegions;
    for(const BensonRegion& region: regions) {
      bool regionActive = true;
      for(Loc borderingHead: region.borderingChainHeads) {
        if(alive.find(borderingHead) == alive.end()) {
          regionActive = false;
          break;
        }
      }
      if(
        regionActive &&
        find(region.vitalForChainHeads.begin(),region.vitalForChainHeads.end(),chainHead) !=
          region.vitalForChainHeads.end()
      )
        vitalRegions.push_back(BensonVitalRegion{region.id,region.points});
    }
    sort(
      vitalRegions.begin(),
      vitalRegions.end(),
      [](const BensonVitalRegion& left, const BensonVitalRegion& right) {
        return left.id < right.id;
      }
    );

    vector<Loc> points = chainPoints(board,chainHead);
    Pattern pattern;
    pattern.kind = Kind::BENSON_ALIVE;
    pattern.schemaVersion = 2;
    pattern.subject.type = SubjectType::CHAIN;
    pattern.subject.color = pla;
    pattern.subject.id = stablePointID("chain:" + color,points,board);
    pattern.subject.points = std::move(points);
    pattern.outcome = Outcome::ALIVE;
    pattern.confidence = Confidence::PROVEN;
    pattern.details = BensonAliveDetails{
      vitalRegions,
      static_cast<int>(vitalRegions.size()),
    };
    patterns.push_back(std::move(pattern));
  }
  return patterns;
}

}

vector<Pattern> detectBensonAlive(const Board& board, const BoardHistory& hist) {
  const bool suicideLegal = hist.suicideLegalForPassAlive();
  vector<Pattern> patterns = detectForPlayer(board,P_BLACK,suicideLegal);
  vector<Pattern> whitePatterns = detectForPlayer(board,P_WHITE,suicideLegal);
  patterns.insert(
    patterns.end(),
    make_move_iterator(whitePatterns.begin()),
    make_move_iterator(whitePatterns.end())
  );
  return patterns;
}

}
