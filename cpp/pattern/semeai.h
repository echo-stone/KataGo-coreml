#ifndef PATTERN_SEMEAI_H
#define PATTERN_SEMEAI_H

#include "pattern.h"
#include "../game/boardhistory.h"

#include <vector>

namespace GoPattern {

// detectSemeai는 격리된 두 chain이 연결된 공유 활로를 갖는 구조 후보를 검출한다.
// 반환 Pattern은 승패·seki가 아니라 sharedLibertyRace의 unsettled 사실만 공개한다.
std::vector<Pattern> detectSemeai(const Board& board, const BoardHistory& hist);

}

#endif
