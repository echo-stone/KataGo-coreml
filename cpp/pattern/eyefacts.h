#ifndef PATTERN_EYEFACTS_H
#define PATTERN_EYEFACTS_H

#include "../game/boardhistory.h"

#include <vector>

namespace GoPattern {

// EyePointFact는 한 점짜리 눈 후보에서 관측한 Board 사실만 보존한다.
// 공개 Pattern 판정이나 연결군 생사 의미를 포함하지 않는다.
struct EyePointFact {
  Loc loc;
  Player owner;
  bool simpleEye;
  bool opponentEntryLegalIgnoringKo;
  std::vector<Loc> hostileDiagonalPoints;
  std::vector<Loc> borderingChainHeads;
};

// extractEyePointFacts는 단일 색 돌과 벽으로 직교 경계가 닫힌 한 점 빈칸을 추출한다.
// board와 hist는 현재 위치와 실제 착수 룰이며, 반환값은 좌표 순서의 비공개 사실 목록이다.
std::vector<EyePointFact> extractEyePointFacts(const Board& board, const BoardHistory& hist);

}

#endif
