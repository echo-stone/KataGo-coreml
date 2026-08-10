#ifndef PATTERN_OWNERSHIPDELTA_H
#define PATTERN_OWNERSHIPDELTA_H

#include "pattern.h"

#include <cstdint>
#include <string>
#include <vector>

namespace GoPattern {

// OWNERSHIP_DELTA_MIN_EDGE_VISITS는 기록 수 child가 충족해야 하는 고정 방문 수다.
static constexpr int64_t OWNERSHIP_DELTA_MIN_EDGE_VISITS = 30;

// OwnershipField는 padding을 제거한 행 우선 ownership 평균·표준편차를 한 관점으로 묶는다.
struct OwnershipField {
  int xSize;
  int ySize;
  Player perspective;
  std::vector<double> mean;
  std::vector<double> pointStdDev;
  std::vector<bool> valid;
};

// OwnershipDeltaThresholds는 development에서 선택할 연결요소 기준을 명시적으로 전달한다.
struct OwnershipDeltaThresholds {
  double magnitudeSeed;
  double magnitudeGrow;
  double uncertaintyMultiplier;
  int minPointCount;
};

// OwnershipDeltaInput은 같은 검색의 root와 기록 수 child ownership을 검출기에 전달한다.
struct OwnershipDeltaInput {
  OwnershipField root;
  OwnershipField child;
  Loc move;
};

// OwnershipDeltaEligibility는 기록 수 강제 child 준비 또는 억제 이유를 값으로 보존한다.
enum class OwnershipDeltaEligibility {
  ELIGIBLE,
  PATTERNS_DISABLED,
  TERMINAL,
  NO_RECORDED_MOVE,
  PASS_MOVE,
  NOT_AUTHORITATIVE_NEXT_MOVE,
  INCLUDE_ALL_MOVES,
  PLAYER_MISMATCH,
  TOO_MANY_INCLUDE_MOVES,
};

// OwnershipDeltaRequestPlan은 기존 includeMoves를 보존한 강제 child 병합 결과다.
struct OwnershipDeltaRequestPlan {
  OwnershipDeltaEligibility eligibility;
  std::vector<Loc> includeMoves;
  int64_t includeMovesMinVisits;
  std::string warning;
};

// planRecordedMoveOwnershipDelta는 기록 수를 중복 없이 병합하고 30 visits·8개 상한을 적용한다.
// includePatterns부터 existingMinVisits까지는 요청 상태이며, 반환값은 검색에 적용할 불변 계획이다.
OwnershipDeltaRequestPlan planRecordedMoveOwnershipDelta(
  bool includePatterns,
  bool ownershipDeltaEnabled,
  bool terminal,
  bool includeAllMoves,
  Player rootPla,
  bool hasRecordedMove,
  bool allowRecordedChild,
  const Move& recordedMove,
  const std::vector<Loc>& existingIncludeMoves,
  int64_t existingMinVisits
);

// detectOwnershipDelta는 명시된 threshold로 같은 부호 4-neighbor hysteresis component를 만든다.
// board는 좌표계, input은 고정 흑 관점 root/child 자료이며, 반환값은 평가기와 같은 순위의 최대 5개 패턴이다.
std::vector<Pattern> detectOwnershipDelta(
  const Board& board,
  const OwnershipDeltaInput& input,
  const OwnershipDeltaThresholds& thresholds
);

}

#endif
