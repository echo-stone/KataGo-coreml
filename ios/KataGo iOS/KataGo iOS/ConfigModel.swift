//
//  ConfigModel.swift
//  KataGo iOS
//
//  Created by Chin-Chang Yang on 2024/7/1.
//

import Foundation

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
    @Published var stoneStyle = defaultStoneStyle
    @Published var showCoordinate = defaultShowCoordinate
    @Published var humanSLRootExploreProbWeightful = defaultHumanSLRootExploreProbWeightful
    @Published var humanSLProfile = defaultHumanSLProfile
}

extension Config {
    func getKataAnalyzeCommand(analysisInterval: Int) -> String {
        return "kata-analyze interval \(analysisInterval) maxmoves \(maxAnalysisMoves) rootInfo true ownership true ownershipStdev true"
    }

    func getKataAnalyzeCommand() -> String {
        return getKataAnalyzeCommand(analysisInterval: analysisInterval)
    }

    func getKataFastAnalyzeCommand() -> String {
        return getKataAnalyzeCommand(analysisInterval: 10);
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
    static let defaultAnalysisWideRootNoise: Float = 0.03125
    static let defaultMaxMessageCharacters = 5000
    static let defaultMaxAnalysisMoves = 50
    static let defaultAnalysisInterval = 50
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
    static let analysisInformationAll = "All"
    static let analysisInformationWinrate = "Winrate"
    static let analysisInformationScore = "Score"

    static let analysisInformations = [analysisInformationWinrate,
                                       analysisInformationScore,
                                       analysisInformationAll]

    func isAnalysisInformationWinrate() -> Bool {
        return Config.analysisInformations[analysisInformation] == Config.analysisInformationWinrate
    }

    func isAnalysisInformationScore() -> Bool {
        return Config.analysisInformations[analysisInformation] == Config.analysisInformationScore
    }
}

extension Config {
    static let fastStoneStyle = "Fast"
    static let classicStoneStyle = "Classic"
    static let stoneStyles = [fastStoneStyle, classicStoneStyle]
    static let defaultStoneStyle = 0

    func isFastStoneStyle() -> Bool {
        return Config.stoneStyles[stoneStyle] == Config.fastStoneStyle
    }

    func isClassicStoneStyle() -> Bool {
        return Config.stoneStyles[stoneStyle] == Config.classicStoneStyle
    }
}

extension Config {
    static let defaultShowCoordinate = false
}

extension Config {
    static let defaultHumanSLRootExploreProbWeightful: Float = 0
}

extension Config {
    static let defaultHumanSLProfile = "rank_9d"
}
