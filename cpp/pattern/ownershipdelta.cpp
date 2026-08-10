#include "ownershipdelta.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

using namespace std;

namespace GoPattern {
namespace {

static constexpr size_t MAX_INCLUDE_MOVES = 8;
static constexpr size_t MAX_COMPONENTS = 5;

struct Component {
  int sign;
  vector<int> indices;
  vector<Loc> points;
  string minimumCoordinate;
  double meanRoot;
  double meanChild;
  double meanDelta;
  double uncertainty;
};

// validateField는 한 ownership 지도의 크기·관점·유한수·범위를 검증한다.
// field는 검사할 값이며, 기대 조건 위반 시 예외를 던지고 반환값은 없다.
static void validateField(const OwnershipField& field) {
  if(field.xSize <= 0 || field.ySize <= 0)
    throw invalid_argument("OwnershipField board size must be positive");
  if(field.perspective != P_BLACK)
    throw invalid_argument("OwnershipField perspective must be black");
  const size_t area = static_cast<size_t>(field.xSize) * static_cast<size_t>(field.ySize);
  if(field.mean.size() != area || field.pointStdDev.size() != area || field.valid.size() != area)
    throw invalid_argument("OwnershipField arrays must match board area");
  for(size_t i = 0; i < area; i++) {
    if(!isfinite(field.mean[i]) || field.mean[i] < -1.0 || field.mean[i] > 1.0)
      throw invalid_argument("OwnershipField mean must be finite and in [-1,1]");
    if(!isfinite(field.pointStdDev[i]) || field.pointStdDev[i] < 0.0 || field.pointStdDev[i] > 1.0)
      throw invalid_argument("OwnershipField pointStdDev must be finite and in [0,1]");
  }
}

// validateThresholds는 offline 평가기와 같은 threshold 범위를 강제한다.
// thresholds는 검사할 설정이며, 위반 시 예외를 던지고 반환값은 없다.
static void validateThresholds(const OwnershipDeltaThresholds& thresholds) {
  if(
    !isfinite(thresholds.magnitudeGrow) ||
    !isfinite(thresholds.magnitudeSeed) ||
    thresholds.magnitudeGrow <= 0.0 ||
    thresholds.magnitudeGrow > thresholds.magnitudeSeed ||
    thresholds.magnitudeSeed > 2.0
  )
    throw invalid_argument("OwnershipDelta magnitude thresholds are invalid");
  if(
    !isfinite(thresholds.uncertaintyMultiplier) ||
    thresholds.uncertaintyMultiplier < 0.0 ||
    thresholds.uncertaintyMultiplier > 10.0
  )
    throw invalid_argument("OwnershipDelta uncertainty multiplier is invalid");
  if(thresholds.minPointCount < 1)
    throw invalid_argument("OwnershipDelta minPointCount must be positive");
}

// rowMajorLoc은 evaluator의 행 우선 index를 KataGo Loc으로 바꾼다.
// index와 xSize는 입력 좌표계이며, 반환값은 같은 교차점의 Loc이다.
static Loc rowMajorLoc(int index, int xSize) {
  return Location::getLoc(index % xSize,index / xSize,xSize);
}

// appendNeighborIndices는 행 우선 index의 판 안 4-neighbor를 덧붙인다.
// index·크기는 좌표계, neighbors는 출력 버퍼이며 반환값은 없다.
static void appendNeighborIndices(int index, int xSize, int ySize, vector<int>& neighbors) {
  const int x = index % xSize;
  const int y = index / xSize;
  if(x > 0)
    neighbors.push_back(index - 1);
  if(x + 1 < xSize)
    neighbors.push_back(index + 1);
  if(y > 0)
    neighbors.push_back(index - xSize);
  if(y + 1 < ySize)
    neighbors.push_back(index + xSize);
}

}

OwnershipDeltaRequestPlan planRecordedMoveOwnershipDelta(
  bool includePatterns,
  bool ownershipDeltaEnabled,
  bool terminal,
  bool includeAllMoves,
  Player rootPla,
  bool hasRecordedMove,
  bool allowRecordedChild,
  const Move& recordedMove,
  const vector<Loc>& existingIncludeMoves,
  int64_t existingMinVisits
) {
  OwnershipDeltaRequestPlan plan{
    OwnershipDeltaEligibility::ELIGIBLE,
    existingIncludeMoves,
    existingMinVisits,
    "",
  };
  if(!includePatterns || !ownershipDeltaEnabled)
    plan.eligibility = OwnershipDeltaEligibility::PATTERNS_DISABLED;
  else if(terminal)
    plan.eligibility = OwnershipDeltaEligibility::TERMINAL;
  else if(!hasRecordedMove)
    plan.eligibility = OwnershipDeltaEligibility::NO_RECORDED_MOVE;
  else if(recordedMove.loc == Board::PASS_LOC)
    plan.eligibility = OwnershipDeltaEligibility::PASS_MOVE;
  else if(!allowRecordedChild)
    plan.eligibility = OwnershipDeltaEligibility::NOT_AUTHORITATIVE_NEXT_MOVE;
  else if(includeAllMoves)
    plan.eligibility = OwnershipDeltaEligibility::INCLUDE_ALL_MOVES;
  else if(recordedMove.pla != rootPla)
    plan.eligibility = OwnershipDeltaEligibility::PLAYER_MISMATCH;
  if(plan.eligibility != OwnershipDeltaEligibility::ELIGIBLE)
    return plan;

  plan.includeMoves.clear();
  for(Loc loc: existingIncludeMoves) {
    if(find(plan.includeMoves.begin(),plan.includeMoves.end(),loc) == plan.includeMoves.end())
      plan.includeMoves.push_back(loc);
  }
  if(find(plan.includeMoves.begin(),plan.includeMoves.end(),recordedMove.loc) == plan.includeMoves.end())
    plan.includeMoves.push_back(recordedMove.loc);
  if(plan.includeMoves.size() > MAX_INCLUDE_MOVES) {
    plan.eligibility = OwnershipDeltaEligibility::TOO_MANY_INCLUDE_MOVES;
    plan.includeMoves = existingIncludeMoves;
    plan.warning = "OwnershipDelta suppressed because unique includeMoves including the recorded move exceeds 8";
    return plan;
  }
  plan.includeMovesMinVisits = max(existingMinVisits,OWNERSHIP_DELTA_MIN_EDGE_VISITS);
  return plan;
}

vector<Pattern> detectOwnershipDelta(
  const Board& board,
  const OwnershipDeltaInput& input,
  const OwnershipDeltaThresholds& thresholds
) {
  validateField(input.root);
  validateField(input.child);
  validateThresholds(thresholds);
  if(
    input.root.xSize != input.child.xSize ||
    input.root.ySize != input.child.ySize ||
    input.root.xSize != board.x_size ||
    input.root.ySize != board.y_size
  )
    throw invalid_argument("OwnershipDelta root, child, and board sizes must match");
  if(input.move == Board::NULL_LOC || input.move == Board::PASS_LOC)
    throw invalid_argument("OwnershipDelta move must be a board point");
  const int moveX = Location::getX(input.move,board.x_size);
  const int moveY = Location::getY(input.move,board.x_size);
  if(moveX < 0 || moveX >= board.x_size || moveY < 0 || moveY >= board.y_size)
    throw invalid_argument("OwnershipDelta move must be inside the board");

  const int area = board.x_size * board.y_size;
  vector<double> delta(area);
  vector<double> pointUncertainty(area);
  vector<bool> grow(area,false);
  vector<bool> seed(area,false);
  for(int index = 0; index < area; index++) {
    if(!input.root.valid[index] || !input.child.valid[index])
      continue;
    delta[index] = input.child.mean[index] - input.root.mean[index];
    pointUncertainty[index] = sqrt(
      input.root.pointStdDev[index] * input.root.pointStdDev[index] +
      input.child.pointStdDev[index] * input.child.pointStdDev[index]
    );
    const double magnitude = abs(delta[index]);
    const bool uncertaintyEligible =
      magnitude >= thresholds.uncertaintyMultiplier * pointUncertainty[index];
    grow[index] =
      delta[index] != 0.0 &&
      magnitude >= thresholds.magnitudeGrow &&
      uncertaintyEligible;
    seed[index] = grow[index] && magnitude >= thresholds.magnitudeSeed;
  }

  vector<bool> visited(area,false);
  vector<Component> components;
  for(int start = 0; start < area; start++) {
    if(!grow[start] || visited[start])
      continue;
    const int sign = delta[start] > 0.0 ? 1 : -1;
    vector<int> stack{start};
    vector<int> indices;
    visited[start] = true;
    bool containsSeed = false;
    while(!stack.empty()) {
      const int index = stack.back();
      stack.pop_back();
      indices.push_back(index);
      containsSeed = containsSeed || seed[index];
      vector<int> neighbors;
      appendNeighborIndices(index,board.x_size,board.y_size,neighbors);
      for(int neighbor: neighbors) {
        if(
          grow[neighbor] &&
          !visited[neighbor] &&
          (delta[neighbor] > 0.0 ? 1 : -1) == sign
        ) {
          visited[neighbor] = true;
          stack.push_back(neighbor);
        }
      }
    }
    sort(indices.begin(),indices.end());
    if(!containsSeed || indices.size() < static_cast<size_t>(thresholds.minPointCount))
      continue;

    Component component;
    component.sign = sign;
    component.indices = indices;
    component.meanRoot = 0.0;
    component.meanChild = 0.0;
    component.meanDelta = 0.0;
    double uncertaintySquaredSum = 0.0;
    vector<pair<string,Loc>> coordinates;
    coordinates.reserve(indices.size());
    for(int index: indices) {
      component.meanRoot += input.root.mean[index];
      component.meanChild += input.child.mean[index];
      component.meanDelta += delta[index];
      uncertaintySquaredSum += pointUncertainty[index] * pointUncertainty[index];
      const Loc loc = rowMajorLoc(index,board.x_size);
      coordinates.emplace_back(Location::toString(loc,board),loc);
    }
    const double count = static_cast<double>(indices.size());
    component.meanRoot /= count;
    component.meanChild /= count;
    component.meanDelta /= count;
    component.uncertainty = sqrt(uncertaintySquaredSum / count);
    sort(coordinates.begin(),coordinates.end());
    component.minimumCoordinate = coordinates.front().first;
    for(const auto& coordinate: coordinates)
      component.points.push_back(coordinate.second);
    components.push_back(std::move(component));
  }

  sort(
    components.begin(),
    components.end(),
    [](const Component& a, const Component& b) {
      const double aScore = abs(a.meanDelta) * sqrt(static_cast<double>(a.indices.size()));
      const double bScore = abs(b.meanDelta) * sqrt(static_cast<double>(b.indices.size()));
      if(aScore != bScore)
        return aScore > bScore;
      if(abs(a.meanDelta) != abs(b.meanDelta))
        return abs(a.meanDelta) > abs(b.meanDelta);
      if(a.indices.size() != b.indices.size())
        return a.indices.size() > b.indices.size();
      if(a.sign != b.sign)
        return a.sign > b.sign;
      return a.minimumCoordinate < b.minimumCoordinate;
    }
  );
  if(components.size() > MAX_COMPONENTS)
    components.resize(MAX_COMPONENTS);

  vector<Pattern> patterns;
  patterns.reserve(components.size());
  for(const Component& component: components) {
    const Player color = component.sign > 0 ? P_BLACK : P_WHITE;
    Pattern pattern;
    pattern.kind = Kind::OWNERSHIP_DELTA;
    pattern.schemaVersion = 1;
    pattern.subject = PatternSubject{
      SubjectType::REGION,
      color,
      "region:" + PlayerIO::playerToStringShort(color) + ":" + component.minimumCoordinate,
      component.points,
    };
    pattern.outcome = Outcome::CHANGED;
    pattern.confidence = Confidence::MEDIUM;
    pattern.details = OwnershipDeltaDetails{
      input.move,
      static_cast<int>(component.points.size()),
      component.meanRoot,
      component.meanChild,
      component.meanDelta,
      component.uncertainty,
    };
    patterns.push_back(std::move(pattern));
  }
  return patterns;
}

}
