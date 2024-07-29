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

    init(sgf: String = "", currentIndex: Int = 0) {
        self.sgf = sgf
        self.currentIndex = currentIndex
    }

    func undo() {
        if (currentIndex > 0) {
            currentIndex = currentIndex - 1
        }
    }
}
