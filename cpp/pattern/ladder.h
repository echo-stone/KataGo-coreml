#ifndef PATTERN_LADDER_H
#define PATTERN_LADDER_H

#include "pattern.h"
#include "../game/boardhistory.h"

#include <vector>

namespace GoPattern {

// LadderDetectionResult는 공개 가능한 사다리 패턴과 예산으로 생략한 후보 수를 반환한다.
struct LadderDetectionResult {
  std::vector<Pattern> patterns;
  int incompleteSearches;
};

// detectLadders는 현재 차례와 대상 chain 활로 수가 맞는 사다리 후보만 판정한다.
// board와 hist는 분석 위치이며, 반환값은 완결·패 의존 결과와 예산 미완결 수다.
LadderDetectionResult detectLadders(const Board& board, const BoardHistory& hist);

}

#endif
