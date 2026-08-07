#ifndef PATTERN_NAKADE_H
#define PATTERN_NAKADE_H

#include "pattern.h"
#include "../game/boardhistory.h"

#include <vector>

namespace GoPattern {

// detectNakade는 유일 급소 catalog에 맞는 닫힌 국소 눈 공간을 검출한다.
// 반환 Pattern은 surrounding chain의 생사가 아니라 localEyeSpace 의미만 가진다.
std::vector<Pattern> detectNakade(const Board& board, const BoardHistory& hist);

}

#endif
