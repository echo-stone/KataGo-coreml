#include "detector.h"
#include "benson.h"
#include "ladder.h"

#include <algorithm>

using namespace std;

namespace GoPattern {
namespace {

static constexpr size_t MAX_PATTERNS = 256;

// confidenceRank는 공개 confidence를 높은 우선순위의 작은 정수로 변환한다.
// confidence는 proven/high/medium/low 중 하나이며, 반환값은 결정적 정렬 우선순위다.
static int confidenceRank(Confidence confidence) {
  if(confidence == Confidence::PROVEN)
    return 0;
  if(confidence == Confidence::HIGH)
    return 1;
  if(confidence == Confidence::MEDIUM)
    return 2;
  ASSERT_UNREACHABLE;
}

}

DetectionResult detect(const Board& board, const BoardHistory& hist) {
  DetectionResult result;
  result.patterns = detectBensonAlive(board,hist);
  LadderDetectionResult ladders = detectLadders(board,hist);
  for(Pattern& ladder: ladders.patterns) {
    bool conflictsWithBenson = false;
    if(ladder.outcome == Outcome::ATTACKER_WINS) {
      for(const Pattern& existing: result.patterns) {
        if(
          existing.kind == Kind::BENSON_ALIVE &&
          existing.subject.id == ladder.subject.id
        ) {
          conflictsWithBenson = true;
          break;
        }
      }
    }
    if(!conflictsWithBenson)
      result.patterns.push_back(std::move(ladder));
  }
  if(ladders.incompleteSearches > 0) {
    result.warnings.push_back(
      "Ladder search budget was exhausted for " +
      Global::intToString(ladders.incompleteSearches) +
      " candidate chains"
    );
  }
  sort(
    result.patterns.begin(),
    result.patterns.end(),
    [](const Pattern& a, const Pattern& b) {
      int aConfidence = confidenceRank(a.confidence);
      int bConfidence = confidenceRank(b.confidence);
      if(aConfidence != bConfidence)
        return aConfidence < bConfidence;
      if(a.kind != b.kind)
        return static_cast<int>(a.kind) < static_cast<int>(b.kind);
      if(a.subject.id != b.subject.id)
        return a.subject.id < b.subject.id;
      return static_cast<int>(a.outcome) < static_cast<int>(b.outcome);
    }
  );
  result.truncated = result.patterns.size() > MAX_PATTERNS;
  if(result.truncated)
    result.patterns.resize(MAX_PATTERNS);
  return result;
}

}
