#include "bensonoracle.h"

#include <algorithm>
#include <cassert>
#include <set>
#include <sstream>

using namespace std;

namespace TestPatternOracle {
namespace {

struct RawChain {
  int color;
  vector<int> points;
  set<int> liberties;
};

struct PlayerRegion {
  vector<int> points;
  vector<int> borderingChainIndexes;
  vector<int> vitalForChainIndexes;
};

static vector<int> neighbors(int point, int width, int height) {
  const int x = point % width;
  const int y = point / width;
  vector<int> result;
  if(x > 0)
    result.push_back(point-1);
  if(x+1 < width)
    result.push_back(point+1);
  if(y > 0)
    result.push_back(point-width);
  if(y+1 < height)
    result.push_back(point+width);
  return result;
}

static vector<RawChain> buildAllChains(
  const vector<int>& colors,
  int width,
  int height
) {
  vector<bool> visited(colors.size(),false);
  vector<RawChain> chains;
  for(int initial = 0; initial < static_cast<int>(colors.size()); initial++) {
    if(colors[initial] == 0 || visited[initial])
      continue;
    RawChain chain;
    chain.color = colors[initial];
    vector<int> queue{initial};
    visited[initial] = true;
    for(size_t queueIndex = 0; queueIndex < queue.size(); queueIndex++) {
      const int point = queue[queueIndex];
      chain.points.push_back(point);
      for(int adjacent: neighbors(point,width,height)) {
        if(colors[adjacent] == 0)
          chain.liberties.insert(adjacent);
        else if(colors[adjacent] == chain.color && !visited[adjacent]) {
          visited[adjacent] = true;
          queue.push_back(adjacent);
        }
      }
    }
    sort(chain.points.begin(),chain.points.end());
    chains.push_back(std::move(chain));
  }
  return chains;
}

static bool contains(const vector<int>& values, int value) {
  return find(values.begin(),values.end(),value) != values.end();
}

static vector<PlayerRegion> buildPlayerRegions(
  const vector<int>& colors,
  int width,
  int height,
  int player,
  bool suicideLegal,
  const vector<RawChain>& chains,
  const vector<int>& chainAt
) {
  const int opponent = 3-player;
  vector<bool> visited(colors.size(),false);
  vector<PlayerRegion> regions;
  for(int initial = 0; initial < static_cast<int>(colors.size()); initial++) {
    if(colors[initial] != 0 || visited[initial])
      continue;
    PlayerRegion region;
    vector<int> queue{initial};
    visited[initial] = true;
    for(size_t queueIndex = 0; queueIndex < queue.size(); queueIndex++) {
      const int point = queue[queueIndex];
      region.points.push_back(point);
      for(int adjacent: neighbors(point,width,height)) {
        if(colors[adjacent] == player) {
          const int chainIndex = chainAt[adjacent];
          assert(chainIndex >= 0);
          if(!contains(region.borderingChainIndexes,chainIndex))
            region.borderingChainIndexes.push_back(chainIndex);
        }
        else if(
          (colors[adjacent] == 0 || colors[adjacent] == opponent) &&
          !visited[adjacent]
        ) {
          visited[adjacent] = true;
          queue.push_back(adjacent);
        }
      }
    }
    sort(region.points.begin(),region.points.end());
    sort(region.borderingChainIndexes.begin(),region.borderingChainIndexes.end());

    for(int chainIndex: region.borderingChainIndexes) {
      assert(chains[chainIndex].color == player);
      bool vital = true;
      for(int point: region.points) {
        if(!suicideLegal && colors[point] == opponent)
          continue;
        bool adjacentToChain = false;
        for(int adjacent: neighbors(point,width,height)) {
          if(chainAt[adjacent] == chainIndex) {
            adjacentToChain = true;
            break;
          }
        }
        if(!adjacentToChain) {
          vital = false;
          break;
        }
      }
      if(vital)
        region.vitalForChainIndexes.push_back(chainIndex);
    }
    regions.push_back(std::move(region));
  }
  return regions;
}

static bool regionActive(const PlayerRegion& region, const vector<bool>& alive) {
  for(int chainIndex: region.borderingChainIndexes) {
    if(!alive[chainIndex])
      return false;
  }
  return true;
}

static string joinStrings(const vector<string>& values, const string& delimiter) {
  ostringstream out;
  for(size_t index = 0; index < values.size(); index++) {
    if(index > 0)
      out << delimiter;
    out << values[index];
  }
  return out.str();
}

static vector<string> sortedGtpPoints(
  const vector<int>& points,
  int width,
  int height
) {
  vector<string> result;
  for(int point: points)
    result.push_back(gtpPoint(point,width,height));
  sort(result.begin(),result.end());
  return result;
}

}

bool hasZeroLibertyChain(
  const vector<int>& colors,
  int width,
  int height
) {
  assert(width > 0 && height > 0);
  assert(static_cast<int>(colors.size()) == width*height);
  for(const RawChain& chain: buildAllChains(colors,width,height)) {
    if(chain.liberties.empty())
      return true;
  }
  return false;
}

BensonResult evaluateBenson(
  const vector<int>& colors,
  int width,
  int height,
  bool suicideLegal
) {
  assert(width > 0 && height > 0);
  assert(static_cast<int>(colors.size()) == width*height);
  for(int color: colors)
    assert(color >= 0 && color <= 2);

  const vector<RawChain> rawChains = buildAllChains(colors,width,height);
  vector<int> chainAt(colors.size(),-1);
  for(size_t chainIndex = 0; chainIndex < rawChains.size(); chainIndex++) {
    for(int point: rawChains[chainIndex].points)
      chainAt[point] = static_cast<int>(chainIndex);
  }

  BensonResult result;
  result.chains.reserve(rawChains.size());
  for(const RawChain& chain: rawChains)
    result.chains.push_back(BensonChain{chain.color,chain.points,false,{}});

  for(int player: {1,2}) {
    const vector<PlayerRegion> regions = buildPlayerRegions(
      colors,width,height,player,suicideLegal,rawChains,chainAt
    );
    vector<bool> alive(rawChains.size(),false);
    for(size_t chainIndex = 0; chainIndex < rawChains.size(); chainIndex++) {
      if(rawChains[chainIndex].color == player)
        alive[chainIndex] = true;
    }

    while(true) {
      vector<int> removed;
      for(size_t chainIndex = 0; chainIndex < rawChains.size(); chainIndex++) {
        if(!alive[chainIndex] || rawChains[chainIndex].color != player)
          continue;
        int vitalCount = 0;
        for(const PlayerRegion& region: regions) {
          if(
            regionActive(region,alive) &&
            contains(region.vitalForChainIndexes,static_cast<int>(chainIndex))
          )
            vitalCount += 1;
        }
        if(vitalCount < 2)
          removed.push_back(static_cast<int>(chainIndex));
      }
      if(removed.empty())
        break;
      for(int chainIndex: removed)
        alive[chainIndex] = false;
    }

    for(size_t chainIndex = 0; chainIndex < rawChains.size(); chainIndex++) {
      if(rawChains[chainIndex].color != player)
        continue;
      BensonChain& chain = result.chains[chainIndex];
      chain.alive = alive[chainIndex];
      if(!chain.alive)
        continue;
      for(const PlayerRegion& region: regions) {
        if(
          regionActive(region,alive) &&
          contains(region.vitalForChainIndexes,static_cast<int>(chainIndex))
        ) {
          chain.vitalRegions.push_back(BensonRegion{
            region.points,
            region.borderingChainIndexes,
            region.vitalForChainIndexes,
          });
        }
      }
    }
  }
  return result;
}

string chainKey(const BensonChain& chain) {
  ostringstream out;
  out << chain.color << ":";
  for(size_t index = 0; index < chain.points.size(); index++) {
    if(index > 0)
      out << ",";
    out << chain.points[index];
  }
  return out.str();
}

string gtpPoint(int point, int width, int height) {
  assert(point >= 0 && point < width*height);
  const int x = point % width;
  const int y = point / width;
  assert(x < 25);
  const char column = static_cast<char>('A' + x + (x >= 8 ? 1 : 0));
  return string(1,column) + to_string(height-y);
}

string canonicalPatternKey(
  const BensonChain& chain,
  int width,
  int height
) {
  assert(chain.alive);
  const string color = chain.color == 1 ? "B" : "W";
  const vector<string> chainPoints = sortedGtpPoints(chain.points,width,height);
  vector<string> regionKeys;
  for(const BensonRegion& region: chain.vitalRegions) {
    const vector<string> points = sortedGtpPoints(region.points,width,height);
    regionKeys.push_back(
      "region:" + color + ":" + points[0] + "=" + joinStrings(points,",")
    );
  }
  sort(regionKeys.begin(),regionKeys.end());
  ostringstream out;
  out
    << "bensonAlive|2|chain|" << color
    << "|chain:" << color << ":" << chainPoints[0]
    << "|" << joinStrings(chainPoints,",")
    << "|alive|proven|" << regionKeys.size()
    << "|" << joinStrings(regionKeys,";");
  return out.str();
}

}
