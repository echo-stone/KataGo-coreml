#ifndef COMMAND_ANALYSIS_H_
#define COMMAND_ANALYSIS_H_

namespace Analysis {
  // 분석 요청이 검색 트리 전체의 ownership map을 필요로 하는지 판정한다.
  // 패턴 검출은 Board/History만 사용하므로 includePatterns와 룰 대체 억제 여부는 비용 판정에 영향을 주지 않는다.
  bool shouldAlwaysIncludeOwnerMap(
    bool includeOwnership,
    bool includeOwnershipStdev,
    bool includeMovesOwnership,
    bool includeMovesOwnershipStdev,
    bool includePatterns,
    bool patternsSuppressedByRulesSubstitution
  );
}

#endif
