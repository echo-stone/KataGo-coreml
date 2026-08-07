#ifndef PATTERN_PATTERN_H
#define PATTERN_PATTERN_H

#include "../game/board.h"
#include "../external/nlohmann_json/json.hpp"

#include <string>
#include <variant>
#include <vector>

namespace GoPattern {

// Kind는 구현과 릴리스 게이트가 끝난 패턴 종류만 포함한다.
enum class Kind {
  BENSON_ALIVE,
  LADDER,
};

// SubjectType은 Pattern 공통 봉투가 지원하는 보드 대상 종류다.
enum class SubjectType {
  CHAIN,
};

// Outcome은 현재 활성 Kind가 반환할 수 있는 판정 결과다.
enum class Outcome {
  ALIVE,
  ATTACKER_WINS,
  DEFENDER_ESCAPES,
  AMBIGUOUS,
};

// Confidence는 검출 근거의 강도를 나타내는 공통 등급이다.
enum class Confidence {
  PROVEN,
  HIGH,
  MEDIUM,
};

// PatternSubject는 검출된 패턴이 귀속되는 보드 대상을 나타낸다.
struct PatternSubject {
  SubjectType type;
  Player color;
  std::string id;
  std::vector<Loc> points;
};

// BensonAliveDetails는 BensonAlive schemaVersion 1의 증명 근거다.
struct BensonAliveDetails {
  std::vector<std::string> vitalRegionIds;
  int eyeCount;
};

// LadderDetails는 현재 차례 기준 사다리 참가자와 선후수, 확인된 축머리를 보존한다.
struct LadderDetails {
  Player attacker;
  Player defender;
  bool defenderFirst;
  Loc ladderBreaker;
};

// PatternDetails는 구현·게이트가 확정된 Kind의 typed details만 포함한다.
using PatternDetails = std::variant<BensonAliveDetails,LadderDetails>;

// Pattern은 Kind와 독립적인 공통 봉투와 Kind별 typed details를 함께 보존한다.
struct Pattern {
  Kind kind;
  int schemaVersion;
  PatternSubject subject;
  Outcome outcome;
  Confidence confidence;
  PatternDetails details;
};

// toJson은 내부 Pattern을 공개 analysis JSON 계약으로 직렬화한다.
// pattern은 직렬화할 패턴이고 board는 GTP 좌표 변환에 사용한다.
// 반환값은 kind별 details를 포함한 공개 JSON 객체다.
nlohmann::json toJson(const Pattern& pattern, const Board& board);

}

#endif
