#ifndef TESTS_PATTERNORACLE_BENSONORACLE_H
#define TESTS_PATTERNORACLE_BENSONORACLE_H

#include <string>
#include <vector>

namespace TestPatternOracle {

// BensonRegion은 제품 Board와 무관한 raw-grid 활력 영역이다.
struct BensonRegion {
  std::vector<int> points;
  std::vector<int> borderingChainIndexes;
  std::vector<int> vitalForChainIndexes;
};

// BensonChain은 한 색 raw-grid chain의 고정점 생존 여부와 certificate를 보존한다.
struct BensonChain {
  int color;
  std::vector<int> points;
  bool alive;
  std::vector<BensonRegion> vitalRegions;
};

// BensonResult는 두 색의 모든 chain을 행 우선 탐색 순서로 반환한다.
struct BensonResult {
  std::vector<BensonChain> chains;
};

// hasZeroLibertyChain은 raw-grid 상태에 활로가 하나도 없는 chain이 있는지 독립 계산한다.
bool hasZeroLibertyChain(
  const std::vector<int>& colors,
  int width,
  int height
);

// evaluateBenson은 raw 0/1/2 격자에서 chain·영역·활력·최대 고정점을 독립 계산한다.
BensonResult evaluateBenson(
  const std::vector<int>& colors,
  int width,
  int height,
  bool suicideLegal
);

// chainKey는 색과 행 우선 좌표 집합으로 raw chain을 결정적으로 식별한다.
std::string chainKey(const BensonChain& chain);

// canonicalPatternKey는 살아 있는 chain의 BensonAlive v2 공개 필드를 결정 문자열로 만든다.
std::string canonicalPatternKey(
  const BensonChain& chain,
  int width,
  int height
);

// gtpPoint는 행 우선 raw 좌표를 I 열을 건너뛴 GTP 좌표로 변환한다.
std::string gtpPoint(int point, int width, int height);

}

#endif
