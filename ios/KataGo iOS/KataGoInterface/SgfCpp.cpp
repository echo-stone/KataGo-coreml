//
//  SgfCpp.cpp
//  KataGoHelper
//
//  Created by Chin-Chang Yang on 2024/7/8.
//

#include "SgfCpp.hpp"
#include "../../../cpp/dataio/sgf.h"

LocCpp::LocCpp() {
    this->x = -1;
    this->y = -1;
    this->pass = true;
}

LocCpp::LocCpp(const int x, const int y) {
    this->x = x;
    this->y = y;
    this->pass = false;
}

LocCpp::LocCpp(const LocCpp& loc) {
    this->x = loc.x;
    this->y = loc.y;
    this->pass = loc.pass;
}

int LocCpp::getX() const {
    return x;
}

int LocCpp::getY() const {
    return y;
}

bool LocCpp::getPass() const {
    return pass;
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

bool MoveCpp::getPass() const {
    return loc.getPass();
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

        auto initializeLocCpp = [&](const Move& move) {
            if (move.loc == Board::PASS_LOC) {
                return LocCpp();
            } else {
                auto x = Location::getX(move.loc, xSize);
                auto y = Location::getY(move.loc, xSize);
                return LocCpp(x, y);
            }
        };

        auto locCpp = initializeLocCpp(move);
        auto player = (move.pla == P_BLACK) ? PlayerCpp::black : PlayerCpp::white;
        auto moveCpp = MoveCpp(locCpp, player);
        return moveCpp;
    }

    return MoveCpp(LocCpp(0, 0), PlayerCpp::black);
}
