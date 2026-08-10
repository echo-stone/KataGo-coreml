#include "detector.h"
#include "benson.h"
#include "eye.h"
#include "ladder.h"
#include "nakade.h"
#include "semeai.h"

#include <algorithm>
#include <iterator>

using namespace std;

namespace GoPattern {
namespace {

static constexpr size_t MAX_PATTERNS = 256;

// sameColoredPointSet은 ID와 무관하게 두 subject가 같은 색·전체 좌표 집합인지 검사한다.
// left와 right는 비교할 subject이며, 반환값은 색과 중복 제거된 좌표 집합이 모두 같을 때 true다.
static bool sameColoredPointSet(const PatternSubject& left, const PatternSubject& right) {
  if(left.color != right.color)
    return false;
  vector<Loc> leftPoints = left.points;
  vector<Loc> rightPoints = right.points;
  sort(leftPoints.begin(),leftPoints.end());
  sort(rightPoints.begin(),rightPoints.end());
  leftPoints.erase(unique(leftPoints.begin(),leftPoints.end()),leftPoints.end());
  rightPoints.erase(unique(rightPoints.begin(),rightPoints.end()),rightPoints.end());
  return leftPoints == rightPoints;
}

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
  vector<Pattern> eyePatterns = detectEyes(board,hist);
  int suppressedEyeCount = 0;
  for(Pattern& eye: eyePatterns) {
    const EyeDetails& details = get<EyeDetails>(eye.details);
    bool conflictsWithBenson = false;
    for(const EyeCapturedChain& captured: details.capturedChains) {
      PatternSubject capturedSubject{
        SubjectType::CHAIN,
        eye.subject.color,
        captured.id,
        captured.points,
      };
      for(const Pattern& existing: result.patterns) {
        if(
          existing.kind == Kind::BENSON_ALIVE &&
          sameColoredPointSet(existing.subject,capturedSubject)
        ) {
          conflictsWithBenson = true;
          break;
        }
      }
      if(conflictsWithBenson)
        break;
    }
    if(conflictsWithBenson)
      suppressedEyeCount += 1;
    else
      result.patterns.push_back(std::move(eye));
  }
  if(suppressedEyeCount > 0) {
    result.warnings.push_back(
      "Eye immediate-capture proof conflicted with BensonAlive for " +
      Global::intToString(suppressedEyeCount) +
      " candidate regions and was suppressed"
    );
  }
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
  vector<Pattern> nakadePatterns = detectNakade(board,hist);
  result.patterns.insert(
    result.patterns.end(),
    make_move_iterator(nakadePatterns.begin()),
    make_move_iterator(nakadePatterns.end())
  );
  vector<Pattern> semeaiPatterns = detectSemeai(board,hist);
  for(Pattern& semeai: semeaiPatterns) {
    const SemeaiDetails& details = get<SemeaiDetails>(semeai.details);
    bool conflictsWithBenson = false;
    for(const Pattern& existing: result.patterns) {
      if(
        existing.kind == Kind::BENSON_ALIVE &&
        (
          existing.subject.id == details.currentPlayerSubjectId ||
          existing.subject.id == details.opponentSubjectId
        )
      ) {
        conflictsWithBenson = true;
        break;
      }
    }
    if(!conflictsWithBenson)
      result.patterns.push_back(std::move(semeai));
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
