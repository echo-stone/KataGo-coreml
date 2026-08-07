#include "semeai.h"

#include <algorithm>
#include <cassert>
#include <utility>

using namespace std;

namespace GoPattern {
namespace {

// ChainInfo는 한 Board chain의 전체 돌·활로와 결정적 공개 ID를 보존한다.
struct ChainInfo {
  Loc head;
  Player color;
  vector<Loc> points;
  vector<Loc> liberties;
  string subjectId;
};

// appendUnique는 좌표 목록에 아직 없는 값만 추가한다.
static void appendUnique(vector<Loc>& values, Loc value) {
  if(find(values.begin(),values.end(),value) == values.end())
    values.push_back(value);
}

// stablePointID는 좌표 집합의 사전순 최소 GTP 좌표로 결정적 ID를 만든다.
static string stablePointID(const string& prefix, const vector<Loc>& points, const Board& board) {
  assert(!points.empty());
  string anchor = Location::toString(points[0],board);
  for(size_t index = 1; index < points.size(); index++)
    anchor = min(anchor,Location::toString(points[index],board));
  return prefix + ":" + anchor;
}

// collectChainInfo는 원형 chain 목록을 펼치며 전체 활로를 중복 없이 수집한다.
static ChainInfo collectChainInfo(const Board& board, Loc head) {
  ChainInfo chain;
  chain.head = head;
  chain.color = board.colors[head];
  Loc loc = head;
  do {
    chain.points.push_back(loc);
    for(int direction = 0; direction < 4; direction++) {
      Loc adjacent = loc + board.adj_offsets[direction];
      if(board.colors[adjacent] == C_EMPTY)
        appendUnique(chain.liberties,adjacent);
    }
    loc = board.next_in_chain[loc];
  } while(loc != head);
  const string color = chain.color == P_BLACK ? "B" : "W";
  chain.subjectId = stablePointID("chain:" + color,chain.points,board);
  return chain;
}

// intersectLiberties는 두 chain이 함께 사용하는 활로를 반환한다.
static vector<Loc> intersectLiberties(const ChainInfo& left, const ChainInfo& right) {
  vector<Loc> shared;
  for(Loc liberty: left.liberties) {
    if(find(right.liberties.begin(),right.liberties.end(),liberty) != right.liberties.end())
      shared.push_back(liberty);
  }
  return shared;
}

// isConnectedRegion은 공유 활로 집합이 직교 연결된 단일 region인지 검사한다.
static bool isConnectedRegion(const vector<Loc>& points, const Board& board) {
  if(points.empty())
    return false;
  vector<Loc> reached;
  reached.push_back(points[0]);
  for(size_t index = 0; index < reached.size(); index++) {
    Loc loc = reached[index];
    for(int direction = 0; direction < 4; direction++) {
      Loc adjacent = loc + board.adj_offsets[direction];
      if(
        find(points.begin(),points.end(),adjacent) != points.end() &&
        find(reached.begin(),reached.end(),adjacent) == reached.end()
      )
        reached.push_back(adjacent);
    }
  }
  return reached.size() == points.size();
}

// isAllowedChain은 돌 좌표가 두 참가 chain 중 하나인지 검사한다.
static bool isAllowedChain(
  const Board& board,
  Loc stone,
  const ChainInfo& black,
  const ChainInfo& white
) {
  if(board.colors[stone] != P_BLACK && board.colors[stone] != P_WHITE)
    return true;
  Loc head = board.chain_head[stone];
  return head == black.head || head == white.head;
}

// isIsolatedPair는 참가 chain과 그 전체 활로에 제3 chain이 접촉하지 않는지 검사한다.
static bool isIsolatedPair(
  const Board& board,
  const ChainInfo& black,
  const ChainInfo& white
) {
  vector<Loc> participantPoints = black.points;
  participantPoints.insert(
    participantPoints.end(),
    white.points.begin(),
    white.points.end()
  );
  for(Loc point: participantPoints) {
    for(int direction = 0; direction < 4; direction++) {
      Loc adjacent = point + board.adj_offsets[direction];
      if(!isAllowedChain(board,adjacent,black,white))
        return false;
    }
  }

  vector<Loc> liberties = black.liberties;
  for(Loc liberty: white.liberties)
    appendUnique(liberties,liberty);
  for(Loc liberty: liberties) {
    for(int direction = 0; direction < 4; direction++) {
      Loc adjacent = liberty + board.adj_offsets[direction];
      if(!isAllowedChain(board,adjacent,black,white))
        return false;
    }
  }
  return true;
}

}

vector<Pattern> detectSemeai(const Board& board, const BoardHistory& hist) {
  vector<ChainInfo> chains;
  for(int y = 0; y < board.y_size; y++) {
    for(int x = 0; x < board.x_size; x++) {
      Loc loc = Location::getLoc(x,y,board.x_size);
      if(
        (board.colors[loc] == P_BLACK || board.colors[loc] == P_WHITE) &&
        board.chain_head[loc] == loc
      )
        chains.push_back(collectChainInfo(board,loc));
    }
  }

  vector<Pattern> patterns;
  for(size_t leftIndex = 0; leftIndex < chains.size(); leftIndex++) {
    for(size_t rightIndex = leftIndex+1; rightIndex < chains.size(); rightIndex++) {
      const ChainInfo& left = chains[leftIndex];
      const ChainInfo& right = chains[rightIndex];
      if(left.color == right.color)
        continue;
      if(
        left.liberties.size() < 2 || left.liberties.size() > 4 ||
        right.liberties.size() < 2 || right.liberties.size() > 4
      )
        continue;

      const ChainInfo& black = left.color == P_BLACK ? left : right;
      const ChainInfo& white = left.color == P_WHITE ? left : right;
      vector<Loc> shared = intersectLiberties(black,white);
      if(shared.empty() || !isConnectedRegion(shared,board))
        continue;
      if(!isIsolatedPair(board,black,white))
        continue;

      const ChainInfo& current =
        hist.presumedNextMovePla == P_BLACK ? black : white;
      const ChainInfo& opponent =
        hist.presumedNextMovePla == P_BLACK ? white : black;
      if(
        hist.presumedNextMovePla != P_BLACK &&
        hist.presumedNextMovePla != P_WHITE
      )
        continue;

      Pattern pattern;
      pattern.kind = Kind::SEMEAI;
      pattern.schemaVersion = 1;
      pattern.subject.type = SubjectType::REGION;
      pattern.subject.color = C_EMPTY;
      pattern.subject.id = stablePointID("region:E",shared,board);
      pattern.subject.points = std::move(shared);
      pattern.outcome = Outcome::UNSETTLED;
      pattern.confidence = Confidence::MEDIUM;
      const int sharedCount = static_cast<int>(pattern.subject.points.size());
      pattern.details = SemeaiDetails{
        hist.presumedNextMovePla,
        current.subjectId,
        opponent.subjectId,
        static_cast<int>(current.liberties.size())-sharedCount,
        static_cast<int>(opponent.liberties.size())-sharedCount,
        sharedCount,
      };
      patterns.push_back(std::move(pattern));
    }
  }
  return patterns;
}

}
