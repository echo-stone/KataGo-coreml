//
//  SgfHelper.swift
//  KataGoInterface
//
//  Created by Chin-Chang Yang on 2024/7/8.
//

import Foundation

public struct Location {
    public let x: Int
    public let y: Int

    public init(x: Int, y: Int) {
        self.x = x
        self.y = y
    }
}

public enum Player {
    case black
    case white
}

public struct Move {
    public let location: Location
    public let player: Player

    public init(location: Location, player: Player) {
        self.location = location
        self.player = player
    }
}

public class SgfHelper {
    let sgfCpp: SgfCpp

    public init(sgf: String) {
        sgfCpp = SgfCpp(std.string(sgf))
    }

    public func getMove(at index: Int) -> Move? {
        if sgfCpp.isValidIndex(Int32(index)) {
            let moveCpp = sgfCpp.getMoveAt(Int32(index))
            let location = Location(x: Int(moveCpp.x), y: Int(moveCpp.y))
            let player: Player = (moveCpp.player == PlayerCpp.black) ? .black : .white
            let move = Move(location: location, player: player)
            return move
        } else {
            return nil
        }
    }

    public func getLastMoveIndex() -> Int? {
        return ((sgfCpp.valid) && (sgfCpp.movesSize > 0)) ? Int(sgfCpp.movesSize - 1) : nil
    }
}
