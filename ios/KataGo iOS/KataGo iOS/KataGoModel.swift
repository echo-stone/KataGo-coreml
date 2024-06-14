//
//  KataGoModel.swift
//  KataGo iOS
//
//  Created by Chin-Chang Yang on 2023/10/1.
//

import SwiftUI

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
}

enum PlayerColor {
    case black
    case white
}

class PlayerObject: ObservableObject {
    @Published var nextColorForPlayCommand = PlayerColor.black
    @Published var nextColorFromShowBoard = PlayerColor.black
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
    @Published var data: [[String: String]] = []
    @Published var ownership: [BoardPoint: Ownership] = [:]
}

class Config: ObservableObject {
    @Published var isBoardSizeChanged: Bool = false
    @Published var boardWidth: Int = defaultBoardWidth
    @Published var boardHeight: Int = defaultBoardHeight
    @Published var rule: Int = defaultRule
    @Published var komi: Float = defaultKomi
    @Published var playoutDoublingAdvantage: Float = defaultPlayoutDoublingAdvantage
    @Published var analysisWideRootNoise: Float = defaultAnalysisWideRootNoise
    @Published var maxMessageCharacters: Int = defaultMaxMessageCharacters
    @Published var maxAnalysisMoves: Int = defaultMaxAnalysisMoves
    @Published var analysisInterval: Int = defaultAnalysisInterval
    @Published var maxMessageLines: Int = defaultMaxMessageLines
    @Published var analysisInformation: Int = defaultAnalysisInformation
    @Published var hiddenAnalysisVisitRatio: Float = defaultHiddenAnalysisVisitRatio

    func getKataAnalyzeCommand() -> String {
        return "kata-analyze interval \(analysisInterval) maxmoves \(maxAnalysisMoves) ownership true ownershipStdev true"
    }

    func getKataBoardSizeCommand() -> String {
        return "rectangular_boardsize \(boardWidth) \(boardHeight)"
    }

    func getKataKomiCommand() -> String {
        return "komi \(komi)"
    }

    func getKataPlayoutDoublingAdvantageCommand() -> String {
        return "kata-set-param playoutDoublingAdvantage \(playoutDoublingAdvantage)"
    }

    func getKataAnalysisWideRootNoiseCommand() -> String {
        return "kata-set-param analysisWideRootNoise \(analysisWideRootNoise)"
    }
}

extension Config {
    static let defaultBoardWidth = 19
    static let defaultBoardHeight = 19
    static let defaultKomi: Float = 7.0
    static let defaultPlayoutDoublingAdvantage: Float = 0.0
    static let defaultAnalysisWideRootNoise: Float = 0.09375
    static let defaultMaxMessageCharacters = 1000
    static let defaultMaxAnalysisMoves = 36
    static let defaultAnalysisInterval = 20
    static let defaultMaxMessageLines = 1000
    static let defaultHiddenAnalysisVisitRatio: Float = 0.03125
}

extension Config {
    static let defaultRule = 0
    static let rules = ["chinese", "japanese", "korean", "aga", "bga", "new-zealand"]

    func getKataRuleCommand() -> String {
        return "kata-set-rules \(Config.rules[rule])"
    }
}

extension Config {
    static let defaultAnalysisInformation = 0
    static let analysisInformationDefault = "All"
    static let analysisInformationWinrate = "Winrate"
    static let analysisInformationScore = "Score"

    static let analysisInformations = [analysisInformationDefault,
                                       analysisInformationWinrate,
                                       analysisInformationScore]

    func isAnalysisInformationWinrate() -> Bool {
        return Config.analysisInformations[analysisInformation] == Config.analysisInformationWinrate
    }

    func isAnalysisInformationScore() -> Bool {
        return Config.analysisInformations[analysisInformation] == Config.analysisInformationScore
    }
}

struct Dimensions {
    let squareLength: CGFloat
    let squareLengthDiv2: CGFloat
    let squareLengthDiv4: CGFloat
    let squareLengthDiv8: CGFloat
    let squareLengthDiv16: CGFloat
    let boardWidth: CGFloat
    let boardHeight: CGFloat
    let marginWidth: CGFloat
    let marginHeight: CGFloat
    let stoneLength: CGFloat

    init(geometry: GeometryProxy, board: ObservableBoard) {
        self.init(geometry: geometry, width: board.width, height: board.height)
    }

    private init(geometry: GeometryProxy, width: CGFloat, height: CGFloat) {
        let totalWidth = geometry.size.width
        let totalHeight = geometry.size.height
        let squareWidth = totalWidth / (width + 1)
        let squareHeight = totalHeight / (height + 1)
        squareLength = min(squareWidth, squareHeight)
        squareLengthDiv2 = squareLength / 2
        squareLengthDiv4 = squareLength / 4
        squareLengthDiv8 = squareLength / 8
        squareLengthDiv16 = squareLength / 16
        boardWidth = width * squareLength
        boardHeight = height * squareLength
        marginWidth = (totalWidth - boardWidth + squareLength) / 2
        marginHeight = (totalHeight - boardHeight + squareLength) / 2
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
