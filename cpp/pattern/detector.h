#ifndef PATTERN_DETECTOR_H
#define PATTERN_DETECTOR_H

#include "pattern.h"
#include "../game/boardhistory.h"

#include <vector>

namespace GoPattern {

// DetectionResult는 정렬·상한 적용이 끝난 패턴과 절단 여부를 반환한다.
struct DetectionResult {
  std::vector<Pattern> patterns;
  bool truncated;
  std::vector<std::string> warnings;
};

// detect는 활성화된 Kind 검출기를 실행하고 공통 정렬·출력 상한을 적용한다.
// board와 hist는 분석 턴의 정확한 위치 snapshot이다.
// 반환값은 최대 256개의 결정적으로 정렬된 패턴과 절단 여부다.
DetectionResult detect(
  const Board& board,
  const BoardHistory& hist,
  const std::vector<Pattern>& movePatterns = {}
);

}

#endif
