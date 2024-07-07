//
//  GameRecord.swift
//  KataGo iOS
//
//  Created by Chin-Chang Yang on 2024/7/7.
//

import Foundation
import SwiftData

@Model
class GameRecord {
    var sgf: String

    init(sgf: String) {
        self.sgf = sgf
    }
}
