//
//  NavigationContext.swift
//  KataGo iOS
//
//  Created by Chin-Chang Yang on 2024/7/17.
//

import SwiftUI

@Observable
class NavigationContext {
    var selectedGameRecord: GameRecord?

    init(selectedGameRecord: GameRecord? = nil) {
        self.selectedGameRecord = selectedGameRecord
    }
}
