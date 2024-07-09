//
//  SgfCpp.cpp
//  KataGoHelper
//
//  Created by Chin-Chang Yang on 2024/7/8.
//

#include "SgfCpp.hpp"
#include "../../../cpp/dataio/sgf.h"

LocCpp::LocCpp(const int x, const int y) {
    this->x = x;
    this->y = y;
}

LocCpp::LocCpp(const LocCpp& loc) {
    this->x = loc.x;
    this->y = loc.y;
}

int LocCpp::getX() const {
    return x;
}

int LocCpp::getY() const {
    return y;
}

MoveCpp::MoveCpp(const LocCpp& loc, const PlayerCpp player): loc(loc) {
    this->player = player;
}

int MoveCpp::getX() const {
    return loc.getX();
}

int MoveCpp::getY() const {
    return loc.getY();
}

PlayerCpp MoveCpp::getPlayer() const {
    return player;
}

SgfCpp::SgfCpp(const string& str) {
    try {
        sgf = CompactSgf::parse(str);
    } catch (...) {
        // Do nothing
    }
}

bool SgfCpp::getValid() const {
    return sgf != NULL;
}

int SgfCpp::getXSize() const {
    return getValid() ? ((CompactSgf *) sgf)->xSize : 0;
}

int SgfCpp::getYSize() const {
    return getValid() ? ((CompactSgf *) sgf)->ySize : 0;
}

unsigned long SgfCpp::getMovesSize() const {
    return getValid() ? ((CompactSgf *) sgf)->moves.size() : 0;
}

bool SgfCpp::isValidIndex(const int index) const {
    return (index >= 0) && (index < getMovesSize());
}

MoveCpp SgfCpp::getMoveAt(const int index) const {
    if (isValidIndex(index)) {
        auto xSize = getXSize();
        auto& moves = ((CompactSgf *) sgf)->moves;
        auto move = moves[index];
        auto x = Location::getX(move.loc, xSize);
        auto y = Location::getY(move.loc, xSize);
        auto locCpp = LocCpp(x, y);
        auto player = (move.pla == P_BLACK) ? PlayerCpp::black : PlayerCpp::white;
        auto moveCpp = MoveCpp(locCpp, player);
        return moveCpp;
    }

    return MoveCpp(LocCpp(0, 0), PlayerCpp::black);
}
