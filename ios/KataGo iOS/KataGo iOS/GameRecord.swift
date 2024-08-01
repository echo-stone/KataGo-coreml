//
//  GameRecord.swift
//  KataGo iOS
//
//  Created by Chin-Chang Yang on 2024/7/7.
//

import Foundation
import SwiftData

@Model
final class GameRecord {
    var sgf: String
    var currentIndex: Int
    var config: Config
    var name: String
    var lastModificationDate: Date?

    init(sgf: String = "",
         currentIndex: Int = 0,
         config: Config = Config(),
         name: String = "Name",
         lastModificationDate: Date? = Date.now) {
        self.sgf = sgf
        self.currentIndex = currentIndex
        self.config = config
        self.name = name
        self.lastModificationDate = lastModificationDate
    }

    convenience init(gameRecord: GameRecord) {
        self.init(sgf: gameRecord.sgf,
                  currentIndex: gameRecord.currentIndex,
                  config: Config(config: gameRecord.config),
                  name: gameRecord.name)
    }

    func undo() {
        if (currentIndex > 0) {
            currentIndex = currentIndex - 1
        }
    }
}
