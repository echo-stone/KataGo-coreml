//
//  SgfCpp.hpp
//  KataGoHelper
//
//  Created by Chin-Chang Yang on 2024/7/8.
//

#ifndef SgfCpp_hpp
#define SgfCpp_hpp

#include <swift/bridging>
#include <string>

using namespace std;

class LocCpp {
public:
    LocCpp(const int x, const int y);
    LocCpp(const LocCpp& loc);
    int getX() const SWIFT_COMPUTED_PROPERTY;
    int getY() const SWIFT_COMPUTED_PROPERTY;
private:
    int x;
    int y;
};

enum class PlayerCpp {
    black,
    white
};

class MoveCpp {
public:
    MoveCpp(const LocCpp& loc, const PlayerCpp player);
    int getX() const SWIFT_COMPUTED_PROPERTY;
    int getY() const SWIFT_COMPUTED_PROPERTY;
    PlayerCpp getPlayer() const SWIFT_COMPUTED_PROPERTY;
private:
    LocCpp loc;
    PlayerCpp player;
};

class SgfCpp {
public:
    SgfCpp(const string& str);
    bool getValid() const SWIFT_COMPUTED_PROPERTY;
    int getXSize() const SWIFT_COMPUTED_PROPERTY;
    int getYSize() const SWIFT_COMPUTED_PROPERTY;
    unsigned long getMovesSize() const SWIFT_COMPUTED_PROPERTY;
    bool isValidIndex(const int index) const;
    MoveCpp getMoveAt(const int index) const;
private:
    void * sgf;
};

#endif /* SgfCpp_hpp */
