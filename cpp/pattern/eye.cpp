#include "eye.h"
#include "eyefacts.h"

#include <algorithm>
#include <cassert>

using namespace std;

namespace GoPattern {
namespace {

// sortedChainPoints는 전이 전 chain의 전체 돌을 공개 좌표 순서로 고정한다.
// board는 원본 보드, chainHead는 대상 chain 대표 좌표이며, 반환값은 중복 없는 전체 돌 좌표다.
static vector<Loc> sortedChainPoints(const Board& board, Loc chainHead) {
  vector<Loc> points;
  Loc loc = chainHead;
  do {
    points.push_back(loc);
    loc = board.next_in_chain[loc];
  } while(loc != chainHead);
  sort(
    points.begin(),
    points.end(),
    [&board](Loc left, Loc right) {
      return Location::toString(left,board) < Location::toString(right,board);
    }
  );
  points.erase(unique(points.begin(),points.end()),points.end());
  return points;
}

// stableChainId는 전이 전 chain 전체 좌표의 공개 최소점을 사용해 결정적 ID를 만든다.
// owner는 chain 색, points는 정렬된 비어 있지 않은 좌표, board는 좌표계이며, 반환값은 chain ID다.
static string stableChainId(Player owner, const vector<Loc>& points, const Board& board) {
  assert(owner == P_BLACK || owner == P_WHITE);
  assert(!points.empty());
  const string color = owner == P_BLACK ? "B" : "W";
  return "chain:" + color + ":" + Location::toString(points[0],board);
}

// transitionRemovedEntireChain은 실제 수 전이 후 원래 chain의 모든 돌이 제거됐는지 검사한다.
// after는 전이 후 보드, points는 전이 전 전체 돌이며, 반환값은 모두 빈칸이 됐을 때 true다.
static bool transitionRemovedEntireChain(
  const Board& after,
  const vector<Loc>& points
) {
  for(Loc point: points) {
    if(after.colors[point] != C_EMPTY)
      return false;
  }
  return true;
}

}

vector<Pattern> detectEyes(const Board& board, const BoardHistory& hist) {
  vector<Pattern> patterns;
  if(hist.isGameFinished || hist.isPastNormalPhaseEnd)
    return patterns;

  const Player attacker = hist.presumedNextMovePla;
  if(attacker != P_BLACK && attacker != P_WHITE)
    return patterns;

  const vector<EyePointFact> facts = extractEyePointFacts(board,hist);
  for(const EyePointFact& fact: facts) {
    const Player owner = fact.owner;
    if(owner != getOpp(attacker))
      continue;
    vector<Loc> candidateChainHeads;
    for(Loc chainHead: fact.borderingChainHeads) {
      if(
        board.colors[chainHead] == owner &&
        board.getNumLiberties(chainHead) == 1
      )
        candidateChainHeads.push_back(chainHead);
    }
    sort(candidateChainHeads.begin(),candidateChainHeads.end());
    candidateChainHeads.erase(
      unique(candidateChainHeads.begin(),candidateChainHeads.end()),
      candidateChainHeads.end()
    );
    if(candidateChainHeads.empty())
      continue;
    if(!hist.isLegal(board,fact.loc,attacker))
      continue;
    if(hist.isPassForKo(board,fact.loc,attacker))
      continue;

    Board after(board);
    BoardHistory afterHist(hist);
    afterHist.makeBoardMoveAssumeLegal(after,fact.loc,attacker,NULL);

    vector<EyeCapturedChain> capturedChains;
    for(Loc chainHead: candidateChainHeads) {
      vector<Loc> points = sortedChainPoints(board,chainHead);
      if(!transitionRemovedEntireChain(after,points))
        continue;
      capturedChains.push_back(EyeCapturedChain{
        stableChainId(owner,points,board),
        std::move(points),
      });
    }
    sort(
      capturedChains.begin(),
      capturedChains.end(),
      [](const EyeCapturedChain& left, const EyeCapturedChain& right) {
        if(left.id != right.id)
          return left.id < right.id;
        return left.points < right.points;
      }
    );
    capturedChains.erase(
      unique(
        capturedChains.begin(),
        capturedChains.end(),
        [](const EyeCapturedChain& left, const EyeCapturedChain& right) {
          return left.id == right.id && left.points == right.points;
        }
      ),
      capturedChains.end()
    );
    if(capturedChains.empty())
      continue;

    const string ownerColor = owner == P_BLACK ? "B" : "W";
    const string point = Location::toString(fact.loc,board);
    Pattern pattern;
    pattern.kind = Kind::EYE;
    pattern.schemaVersion = 1;
    pattern.subject = PatternSubject{
      SubjectType::REGION,
      owner,
      "region:" + ownerColor + ":" + point,
      {fact.loc},
    };
    pattern.outcome = Outcome::OPPONENT_CAN_CAPTURE_ADJACENT_CHAIN;
    pattern.confidence = Confidence::PROVEN;
    pattern.details = EyeDetails{attacker,std::move(capturedChains)};
    patterns.push_back(std::move(pattern));
  }
  return patterns;
}

}
