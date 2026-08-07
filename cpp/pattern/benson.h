#ifndef PATTERN_BENSON_H
#define PATTERN_BENSON_H

#include "pattern.h"
#include "../game/boardhistory.h"

#include <vector>

namespace GoPattern {

// detectBensonAlive는 현재 국면에서 독립 certificate가 확인된 pass-alive chain을 검출한다.
// board는 분석할 보드이고 hist는 pass-alive 계산에 적용할 룰 상태다.
// 반환값은 confidence=proven인 BensonAlive 패턴 목록이다.
std::vector<Pattern> detectBensonAlive(const Board& board, const BoardHistory& hist);

}

#endif
