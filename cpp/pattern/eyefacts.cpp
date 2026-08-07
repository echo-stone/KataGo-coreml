#include "eyefacts.h"

#include <algorithm>

using namespace std;

namespace GoPattern {
namespace {

// appendUnique는 좌표 목록에 아직 없는 값만 추가한다.
// values는 수정할 목록이고 value는 추가 후보이며, 반환값은 없다.
static void appendUnique(vector<Loc>& values, Loc value) {
  if(find(values.begin(),values.end(),value) == values.end())
    values.push_back(value);
}

}

vector<EyePointFact> extractEyePointFacts(const Board& board, const BoardHistory& hist) {
  vector<EyePointFact> facts;
  for(int y = 0; y < board.y_size; y++) {
    for(int x = 0; x < board.x_size; x++) {
      Loc loc = Location::getLoc(x,y,board.x_size);
      if(board.colors[loc] != C_EMPTY)
        continue;

      Player owner = C_EMPTY;
      vector<Loc> borderingChainHeads;
      bool closedByOneColor = true;
      for(int i = 0; i < 4; i++) {
        Loc adj = loc + board.adj_offsets[i];
        Color color = board.colors[adj];
        if(color == C_WALL)
          continue;
        if(color != P_BLACK && color != P_WHITE) {
          closedByOneColor = false;
          break;
        }
        if(owner == C_EMPTY)
          owner = color;
        else if(owner != color) {
          closedByOneColor = false;
          break;
        }
        appendUnique(borderingChainHeads,board.chain_head[adj]);
      }
      if(!closedByOneColor || owner == C_EMPTY)
        continue;

      vector<Loc> hostileDiagonalPoints;
      Player opponent = getOpp(owner);
      for(int i = 4; i < 8; i++) {
        Loc diagonal = loc + board.adj_offsets[i];
        if(board.colors[diagonal] == opponent)
          hostileDiagonalPoints.push_back(diagonal);
      }
      sort(hostileDiagonalPoints.begin(),hostileDiagonalPoints.end());
      sort(borderingChainHeads.begin(),borderingChainHeads.end());

      facts.push_back(EyePointFact{
        loc,
        owner,
        board.isSimpleEye(loc,owner),
        board.isLegalIgnoringKo(loc,opponent,hist.rules.multiStoneSuicideLegal),
        std::move(hostileDiagonalPoints),
        std::move(borderingChainHeads),
      });
    }
  }
  return facts;
}

}
