#include "pattern.h"

#include <algorithm>
#include <cassert>

using namespace std;
using json = nlohmann::json;

namespace GoPattern {

// kindToString은 활성 Kind enum을 공개 lower camel case 문자열로 바꾼다.
// kind는 변환할 Kind이며, 반환값은 analysis JSON의 kind 값이다.
static string kindToString(Kind kind) {
  switch(kind) {
  case Kind::BENSON_ALIVE:
    return "bensonAlive";
  default:
    ASSERT_UNREACHABLE;
  }
}

// subjectTypeToString은 SubjectType enum을 공개 문자열로 바꾼다.
// type은 변환할 대상 종류이며, 반환값은 analysis JSON의 subject.type 값이다.
static string subjectTypeToString(SubjectType type) {
  switch(type) {
  case SubjectType::CHAIN:
    return "chain";
  default:
    ASSERT_UNREACHABLE;
  }
}

// outcomeToString은 Outcome enum을 공개 lower camel case 문자열로 바꾼다.
// outcome은 변환할 판정이며, 반환값은 analysis JSON의 outcome 값이다.
static string outcomeToString(Outcome outcome) {
  switch(outcome) {
  case Outcome::ALIVE:
    return "alive";
  default:
    ASSERT_UNREACHABLE;
  }
}

// confidenceToString은 Confidence enum을 공개 소문자 문자열로 바꾼다.
// confidence는 변환할 근거 등급이며, 반환값은 analysis JSON의 confidence 값이다.
static string confidenceToString(Confidence confidence) {
  switch(confidence) {
  case Confidence::PROVEN:
    return "proven";
  default:
    ASSERT_UNREACHABLE;
  }
}

// playerToPatternColor는 KataGo Player를 Pattern 계약의 B/W 문자열로 바꾼다.
// pla는 변환할 돌 색이며, 반환값은 B 또는 W다.
static string playerToPatternColor(Player pla) {
  assert(pla == P_BLACK || pla == P_WHITE);
  return pla == P_BLACK ? "B" : "W";
}

// sortedPointStrings는 내부 좌표를 중복 없는 GTP 문자열 사전순으로 정규화한다.
// points는 변환할 보드 좌표이고 board는 원 보드 좌표계를 제공한다.
// 반환값은 정렬되고 중복 제거된 GTP 좌표 목록이다.
static vector<string> sortedPointStrings(const vector<Loc>& points, const Board& board) {
  vector<string> result;
  result.reserve(points.size());
  for(Loc loc: points)
    result.push_back(Location::toString(loc,board));
  sort(result.begin(),result.end());
  result.erase(unique(result.begin(),result.end()),result.end());
  return result;
}

// detailsToJson은 BensonAlive v1의 typed details를 공개 JSON 객체로 직렬화한다.
// details는 검증된 BensonAlive 근거이며, 반환값은 해당 Kind의 details 객체다.
static json detailsToJson(const BensonAliveDetails& details) {
  return json{
    {"vitalRegionIds", details.vitalRegionIds},
    {"eyeCount", details.eyeCount},
  };
}

nlohmann::json toJson(const Pattern& pattern, const Board& board) {
  json subject;
  subject["type"] = subjectTypeToString(pattern.subject.type);
  subject["color"] = playerToPatternColor(pattern.subject.color);
  subject["id"] = pattern.subject.id;
  subject["points"] = sortedPointStrings(pattern.subject.points,board);

  json result;
  result["kind"] = kindToString(pattern.kind);
  result["schemaVersion"] = pattern.schemaVersion;
  result["subject"] = std::move(subject);
  result["outcome"] = outcomeToString(pattern.outcome);
  result["confidence"] = confidenceToString(pattern.confidence);
  result["details"] = visit(
    [](const auto& details) {
      return detailsToJson(details);
    },
    pattern.details
  );
  return result;
}

}
