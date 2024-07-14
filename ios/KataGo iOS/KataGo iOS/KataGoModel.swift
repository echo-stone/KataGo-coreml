//
//  KataGoModel.swift
//  KataGo iOS
//
//  Created by Chin-Chang Yang on 2023/10/1.
//

import SwiftUI
import KataGoInterface

class ObservableBoard: ObservableObject {
    @Published var width: CGFloat = 19
    @Published var height: CGFloat = 19
}

struct BoardPoint: Hashable, Comparable {
    let x: Int
    let y: Int

    static func < (lhs: BoardPoint, rhs: BoardPoint) -> Bool {
        return (lhs.y, lhs.x) < (rhs.y, rhs.x)
    }
}

class Stones: ObservableObject {
    @Published var blackPoints: [BoardPoint] = []
    @Published var whitePoints: [BoardPoint] = []
    @Published var moveOrder: [Character: BoardPoint] = [:]
}

enum PlayerColor {
    case black
    case white
}

class PlayerObject: ObservableObject {
    @Published var nextColorForPlayCommand = PlayerColor.black
    @Published var nextColorFromShowBoard = PlayerColor.black
}

struct AnalysisInfo {
    let visits: Int
    let winrate: Float
    let scoreLead: Float
    let utilityLcb: Float
}

struct Ownership {
    let mean: Float
    let stdev: Float?

    init(mean: Float, stdev: Float?) {
        self.mean = mean
        self.stdev = stdev
    }
}

class Analysis: ObservableObject {
    @Published var nextColorForAnalysis = PlayerColor.white
    @Published var info: [BoardPoint: AnalysisInfo] = [:]
    @Published var ownership: [BoardPoint: Ownership] = [:]

    func clear() {
        info = [:]
        ownership = [:]
    }
}

struct Dimensions {
    let squareLength: CGFloat
    let squareLengthDiv2: CGFloat
    let squareLengthDiv4: CGFloat
    let squareLengthDiv8: CGFloat
    let squareLengthDiv16: CGFloat
    let boardLineStartX: CGFloat
    let boardLineStartY: CGFloat
    let stoneLength: CGFloat
    let width: CGFloat
    let height: CGFloat
    let gobanWidth: CGFloat
    let gobanHeight: CGFloat
    let boardLineBoundWidth: CGFloat
    let boardLineBoundHeight: CGFloat

    init(geometry: GeometryProxy, width: CGFloat, height: CGFloat) {
        let totalWidth = geometry.size.width
        let totalHeight = geometry.size.height
        let squareWidth = totalWidth / (width + 1)
        let squareHeight = totalHeight / (height + 1)
        self.width = width
        self.height = height
        squareLength = min(squareWidth, squareHeight)
        squareLengthDiv2 = squareLength / 2
        squareLengthDiv4 = squareLength / 4
        squareLengthDiv8 = squareLength / 8
        squareLengthDiv16 = squareLength / 16
        let gobanPadding = squareLength / 2
        gobanWidth = (width * squareLength) + gobanPadding
        gobanHeight = (height * squareLength) + gobanPadding
        boardLineBoundWidth = (width - 1) * squareLength
        boardLineBoundHeight = (height - 1) * squareLength
        boardLineStartX = (totalWidth - boardLineBoundWidth) / 2
        boardLineStartY = (totalHeight - boardLineBoundHeight) / 2
        stoneLength = squareLength * 0.95
    }
}

/// Message with a text and an ID
struct Message: Identifiable, Equatable, Hashable {
    /// Identification of this message
    let id = UUID()

    /// Text of this message
    let text: String

    /// Initialize a message with a text and a max length
    /// - Parameters:
    ///   - text: a text
    ///   - maxLength: a max length
    init(text: String, maxLength: Int) {
        self.text = String(text.prefix(maxLength))
    }
}

class MessagesObject: ObservableObject {
    @Published var messages: [Message] = []
}

class GobanState: ObservableObject {
    @Published var paused = false
    @Published var showingAnalysis = true
    @Published var waitingForAnalysis = false
    @Published var requestingClearAnalysis = false

    func requestAnalysis(config: Config) {
        KataGoHelper.sendCommand(config.getKataFastAnalyzeCommand())
        waitingForAnalysis = true
    }
}

class Winrate: ObservableObject {
    @Published var black: Float = 0.5

    var white: Float {
        1 - black
    }
}
