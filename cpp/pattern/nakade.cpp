#include "nakade.h"

#include <algorithm>
#include <cassert>
#include <utility>

using namespace std;

namespace GoPattern {
namespace {

// CatalogEntry는 대칭 정규화 전 기준 좌표와 유일 급소를 보존한다.
struct CatalogEntry {
  NakadeShape shape;
  vector<pair<int,int>> points;
  pair<int,int> vitalPoint;
};

// TransformedPoint는 한 대칭에서의 좌표와 원래 Board 좌표를 함께 보존한다.
struct TransformedPoint {
  int x;
  int y;
  Loc originalLoc;
};

// appendUnique는 좌표 목록에 아직 없는 값만 추가한다.
static void appendUnique(vector<Loc>& values, Loc value) {
  if(find(values.begin(),values.end(),value) == values.end())
    values.push_back(value);
}

// transformedXY는 좌표에 정사각 격자의 8대칭 중 하나를 적용한다.
static pair<int,int> transformedXY(int x, int y, int symmetry) {
  switch(symmetry) {
  case 0: return make_pair(x,y);
  case 1: return make_pair(x,-y);
  case 2: return make_pair(-x,y);
  case 3: return make_pair(-x,-y);
  case 4: return make_pair(y,x);
  case 5: return make_pair(y,-x);
  case 6: return make_pair(-y,x);
  case 7: return make_pair(-y,-x);
  default:
    ASSERT_UNREACHABLE;
  }
}

// catalog는 단일 vitalPoint를 결정적으로 제공할 수 있는 Nakade v1 형태만 반환한다.
static const vector<CatalogEntry>& catalog() {
  static const vector<CatalogEntry> entries = {
    {
      NakadeShape::STRAIGHT_THREE,
      {{0,0},{1,0},{2,0}},
      {1,0},
    },
    {
      NakadeShape::BENT_THREE,
      {{0,0},{0,1},{1,0}},
      {0,0},
    },
    {
      NakadeShape::PYRAMID_FOUR,
      {{0,0},{1,0},{1,1},{2,0}},
      {1,0},
    },
    {
      NakadeShape::STAR_FIVE,
      {{0,1},{1,0},{1,1},{1,2},{2,1}},
      {1,1},
    },
    {
      NakadeShape::BULKY_FIVE,
      {{0,0},{0,1},{0,2},{1,0},{1,1}},
      {0,1},
    },
    {
      NakadeShape::RABBITY_SIX,
      {{0,1},{1,0},{1,1},{1,2},{2,1},{2,2}},
      {1,1},
    },
  };
  return entries;
}

// matchCatalog는 영역을 8대칭·평행이동 정규화해 동결 catalog와 대조한다.
// 일치하면 shape와 원래 Board의 유일 급소를 채우고 true를 반환한다.
static bool matchCatalog(
  const vector<Loc>& region,
  const Board& board,
  NakadeShape& shape,
  Loc& vitalPoint
) {
  for(const CatalogEntry& entry: catalog()) {
    if(entry.points.size() != region.size())
      continue;
    for(int symmetry = 0; symmetry < 8; symmetry++) {
      vector<TransformedPoint> transformed;
      transformed.reserve(region.size());
      int minX = 1000000;
      int minY = 1000000;
      for(Loc loc: region) {
        int x = Location::getX(loc,board.x_size);
        int y = Location::getY(loc,board.x_size);
        pair<int,int> xy = transformedXY(x,y,symmetry);
        minX = min(minX,xy.first);
        minY = min(minY,xy.second);
        transformed.push_back(TransformedPoint{xy.first,xy.second,loc});
      }
      for(TransformedPoint& point: transformed) {
        point.x -= minX;
        point.y -= minY;
      }
      sort(
        transformed.begin(),
        transformed.end(),
        [](const TransformedPoint& left, const TransformedPoint& right) {
          if(left.x != right.x)
            return left.x < right.x;
          return left.y < right.y;
        }
      );

      bool matches = true;
      for(size_t i = 0; i < transformed.size(); i++) {
        if(
          transformed[i].x != entry.points[i].first ||
          transformed[i].y != entry.points[i].second
        ) {
          matches = false;
          break;
        }
      }
      if(!matches)
        continue;

      for(const TransformedPoint& point: transformed) {
        if(point.x == entry.vitalPoint.first && point.y == entry.vitalPoint.second) {
          shape = entry.shape;
          vitalPoint = point.originalLoc;
          return true;
        }
      }
      ASSERT_UNREACHABLE;
    }
  }
  return false;
}

// stablePointID는 좌표 집합의 사전순 최소 GTP 좌표로 결정적 ID를 만든다.
static string stablePointID(const string& prefix, const vector<Loc>& points, const Board& board) {
  assert(!points.empty());
  string anchor = Location::toString(points[0],board);
  for(size_t i = 1; i < points.size(); i++)
    anchor = min(anchor,Location::toString(points[i],board));
  return prefix + ":" + anchor;
}

// chainPoints는 chain의 원형 연결 목록을 전체 좌표 목록으로 펼친다.
static vector<Loc> chainPoints(const Board& board, Loc chainHead) {
  vector<Loc> points;
  Loc loc = chainHead;
  do {
    points.push_back(loc);
    loc = board.next_in_chain[loc];
  } while(loc != chainHead);
  return points;
}

// touchesBoardCorner는 영역이 귀의 특수 전술 범위에 닿는지 검사한다.
static bool touchesBoardCorner(const vector<Loc>& points, const Board& board) {
  for(Loc loc: points) {
    int x = Location::getX(loc,board.x_size);
    int y = Location::getY(loc,board.x_size);
    bool horizontalEdge = x == 0 || x == board.x_size - 1;
    bool verticalEdge = y == 0 || y == board.y_size - 1;
    if(horizontalEdge && verticalEdge)
      return true;
  }
  return false;
}

}

vector<Pattern> detectNakade(const Board& board, const BoardHistory& hist) {
  bool visited[Board::MAX_ARR_SIZE];
  fill(visited,visited+Board::MAX_ARR_SIZE,false);

  vector<Pattern> patterns;
  vector<Loc> queue;
  for(int y = 0; y < board.y_size; y++) {
    for(int x = 0; x < board.x_size; x++) {
      Loc initialLoc = Location::getLoc(x,y,board.x_size);
      if(board.colors[initialLoc] != C_EMPTY || visited[initialLoc])
        continue;

      vector<Loc> region;
      vector<Loc> borderingChainHeads;
      Player defender = C_EMPTY;
      bool mixedBoundary = false;
      queue.clear();
      queue.push_back(initialLoc);
      visited[initialLoc] = true;
      for(size_t queueIndex = 0; queueIndex < queue.size(); queueIndex++) {
        Loc loc = queue[queueIndex];
        region.push_back(loc);
        for(int i = 0; i < 4; i++) {
          Loc adjacent = loc + board.adj_offsets[i];
          Color color = board.colors[adjacent];
          if(color == C_EMPTY) {
            if(!visited[adjacent]) {
              visited[adjacent] = true;
              queue.push_back(adjacent);
            }
            continue;
          }
          if(color == C_WALL)
            continue;
          if(defender == C_EMPTY)
            defender = color;
          else if(defender != color)
            mixedBoundary = true;
          appendUnique(borderingChainHeads,board.chain_head[adjacent]);
        }
      }

      if(
        mixedBoundary ||
        defender == C_EMPTY ||
        borderingChainHeads.size() != 1 ||
        region.size() < 3 ||
        region.size() > 6 ||
        touchesBoardCorner(region,board)
      )
        continue;

      NakadeShape shape;
      Loc vitalPoint = Board::NULL_LOC;
      if(!matchCatalog(region,board,shape,vitalPoint))
        continue;

      const Player attacker = getOpp(defender);
      const bool attackerToMove = hist.presumedNextMovePla == attacker;
      const bool vitalPointLegal = hist.isLegal(board,vitalPoint,attacker);
      const bool reducibleNow = attackerToMove && vitalPointLegal;
      vector<Loc> defenderPoints = chainPoints(board,borderingChainHeads[0]);
      const string defenderColor = defender == P_BLACK ? "B" : "W";

      Pattern pattern;
      pattern.kind = Kind::NAKADE;
      pattern.schemaVersion = 1;
      pattern.subject.type = SubjectType::REGION;
      pattern.subject.color = defender;
      pattern.subject.id = stablePointID("region:" + defenderColor,region,board);
      pattern.subject.points = std::move(region);
      pattern.outcome = reducibleNow ?
        Outcome::ATTACKER_CAN_REDUCE_TO_ONE_EYE :
        Outcome::UNSETTLED;
      pattern.confidence = reducibleNow ? Confidence::HIGH : Confidence::MEDIUM;
      pattern.details = NakadeDetails{
        attacker,
        defender,
        stablePointID("chain:" + defenderColor,defenderPoints,board),
        shape,
        vitalPoint,
      };
      patterns.push_back(std::move(pattern));
    }
  }
  return patterns;
}

}
