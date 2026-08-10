#ifndef PATTERN_EYE_H
#define PATTERN_EYE_H

#include "pattern.h"
#include "../game/boardhistory.h"

#include <vector>

namespace GoPattern {

// detectEyes는 현재 차례 상대의 합법 착수로 인접 소유자 chain이 즉시 제거되는 한 점 영역을 검출한다.
// board와 hist는 분석 턴의 원본 위치이며, 반환값은 실제 전이로 포획이 증명된 Eye v1 목록이다.
std::vector<Pattern> detectEyes(const Board& board, const BoardHistory& hist);

}

#endif
