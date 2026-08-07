#include "ladder.h"

#include <algorithm>
#include <cassert>

using namespace std;

namespace GoPattern {
namespace {

// chainPoints는 chain의 원형 연결 목록을 완전한 좌표 목록으로 펼친다.
// board는 보드이고 chainHead는 대표 좌표이며, 반환값은 모든 돌 좌표다.
static vector<Loc> chainPoints(const Board& board, Loc chainHead) {
  vector<Loc> points;
  Loc loc = chainHead;
  do {
    points.push_back(loc);
    loc = board.next_in_chain[loc];
  } while(loc != chainHead);
  return points;
}

// appendUnique는 공개 Board 사실로 만든 root 착수 후보에서 중복을 제거한다.
static void appendUnique(vector<Loc>& values, Loc value) {
  if(find(values.begin(),values.end(),value) == values.end())
    values.push_back(value);
}

// rootMoves는 첫 수의 superko·encore 금지 경계를 확인할 최소 후보를 열거한다.
static vector<Loc> rootMoves(const Board& board, Loc chainHead, bool defenderFirst) {
  vector<Loc> moves;
  Player defender = board.colors[chainHead];
  Loc stone = chainHead;
  do {
    for(int i = 0; i < 4; i++) {
      Loc adjacent = stone + board.adj_offsets[i];
      if(board.colors[adjacent] == C_EMPTY)
        appendUnique(moves,adjacent);
      else if(
        defenderFirst &&
        board.colors[adjacent] == getOpp(defender) &&
        board.getNumLiberties(adjacent) == 1
      ) {
        Loc opponentStone = board.chain_head[adjacent];
        do {
          for(int j = 0; j < 4; j++) {
            Loc capture = opponentStone + board.adj_offsets[j];
            if(board.colors[capture] == C_EMPTY)
              appendUnique(moves,capture);
          }
          opponentStone = board.next_in_chain[opponentStone];
        } while(opponentStone != board.chain_head[adjacent]);
      }
    }
    stone = board.next_in_chain[stone];
  } while(stone != chainHead);
  return moves;
}

// hasRootHistoryBoundary는 Board 단독 탐색이 보지 못하는 현재 history 금지 수를 찾는다.
static bool hasRootHistoryBoundary(
  const Board& board,
  const BoardHistory& hist,
  Loc chainHead,
  bool defenderFirst
) {
  for(Loc move: rootMoves(board,chainHead,defenderFirst)) {
    if(hist.superKoBanned[move] || hist.koRecapBlocked[move])
      return true;
  }
  return false;
}

// chainID는 chain 점 집합의 사전순 최소 GTP 좌표로 결정적 ID를 만든다.
// board는 좌표계, color는 대상 색, points는 비어 있지 않은 chain이며 반환값은 공개 ID다.
static string chainID(const Board& board, Player color, const vector<Loc>& points) {
  assert(!points.empty());
  string anchor = Location::toString(points[0],board);
  for(size_t i = 1; i < points.size(); i++)
    anchor = min(anchor,Location::toString(points[i],board));
  return "chain:" + string(color == P_BLACK ? "B:" : "W:") + anchor;
}

}

LadderDetectionResult detectLadders(const Board& board, const BoardHistory& hist) {
  LadderDetectionResult result;
  result.incompleteSearches = 0;
  Player nextPlayer = hist.presumedNextMovePla;
  if(nextPlayer != P_BLACK && nextPlayer != P_WHITE)
    return result;

  for(int y = 0; y < board.y_size; y++) {
    for(int x = 0; x < board.x_size; x++) {
      Loc loc = Location::getLoc(x,y,board.x_size);
      Player defender = board.colors[loc];
      if(
        (defender != P_BLACK && defender != P_WHITE) ||
        board.chain_head[loc] != loc
      )
        continue;

      const int liberties = board.getNumLiberties(loc);
      const bool defenderFirst = nextPlayer == defender;
      if((defenderFirst && liberties != 1) || (!defenderFirst && liberties != 2))
        continue;

      Board workingBoard(board);
      vector<Loc> searchBuffer;
      vector<Loc> attackMoves;
      LadderSearchResult searchResult = defenderFirst ?
        workingBoard.searchIsLadderCapturedDetailed(loc,true,searchBuffer) :
        workingBoard.searchIsLadderCapturedAttackerFirst2LibsDetailed(
          loc,
          searchBuffer,
          attackMoves
        );
      if(searchResult.budgetExhausted) {
        result.incompleteSearches += 1;
        continue;
      }
      const bool historyBoundary =
        hasRootHistoryBoundary(board,hist,loc,defenderFirst);

      Outcome outcome;
      Confidence confidence;
      if(searchResult.status == LadderSearchStatus::CAPTURED && !historyBoundary) {
        outcome = Outcome::ATTACKER_WINS;
        confidence = Confidence::HIGH;
      }
      else if(searchResult.status == LadderSearchStatus::ESCAPED && !historyBoundary) {
        outcome = Outcome::DEFENDER_ESCAPES;
        confidence = Confidence::HIGH;
      }
      else if(
        historyBoundary ||
        searchResult.koDependent ||
        searchResult.tacticalCaptureDependent ||
        searchResult.repetitionDependent ||
        searchResult.sequenceLimitReached
      ) {
        outcome = Outcome::AMBIGUOUS;
        confidence = Confidence::MEDIUM;
      }
      else {
        result.incompleteSearches += 1;
        continue;
      }

      vector<Loc> points = chainPoints(board,loc);
      Pattern pattern;
      pattern.kind = Kind::LADDER;
      pattern.schemaVersion = 1;
      pattern.subject.type = SubjectType::CHAIN;
      pattern.subject.color = defender;
      pattern.subject.id = chainID(board,defender,points);
      pattern.subject.points = std::move(points);
      pattern.outcome = outcome;
      pattern.confidence = confidence;
      pattern.details = LadderDetails{
        getOpp(defender),
        defender,
        defenderFirst,
        Board::NULL_LOC,
      };
      result.patterns.push_back(std::move(pattern));
    }
  }
  return result;
}

}
