//
//  ToolbarView.swift
//  KataGo iOS
//
//  Created by Chin-Chang Yang on 2023/10/1.
//

import SwiftUI
import KataGoInterface

struct ToolbarItems: View {
    @EnvironmentObject var player: PlayerObject
    @EnvironmentObject var gobanState: GobanState
    @EnvironmentObject var board: ObservableBoard
    var gameRecord: GameRecord

    var body: some View {
        Group {
            Button(action: passAction) {
                Image(systemName: "hand.raised")
                    .resizable()
                    .scaledToFit()
            }
            .padding()

            Button(action: backwardAction) {
                Image(systemName: "backward.frame")
                    .resizable()
                    .scaledToFit()
            }
            .padding()

            if gobanState.analysisStatus == .pause {
                Button(action: startAnalysisAction) {
                    Image(systemName: "sparkle")
                        .resizable()
                        .scaledToFit()
                }
                .padding()
            } else if gobanState.analysisStatus == .run {
                Button(action: stopAction) {
                    Image(systemName: "sparkle")
                        .resizable()
                        .scaledToFit()
                        .symbolEffect(.variableColor.iterative.reversing, isActive: true)
                }
                .padding()
            } else {
                Button(action: pauseAnalysisAction) {
                    Image(systemName: "sparkle")
                        .resizable()
                        .scaledToFit()
                        .foregroundColor(.red)
                }
                .padding()
            }

            Button(action: forwardAction) {
                Image(systemName: "forward.frame")
                    .resizable()
                    .scaledToFit()
            }
            .padding()

            Button(action: clearBoardAction) {
                Image(systemName: "clear")
                    .resizable()
                    .scaledToFit()
            }
            .padding()
        }
    }

    func passAction() {
        let nextColor = (player.nextColorForPlayCommand == .black) ? "b" : "w"
        let pass = "play \(nextColor) pass"
        KataGoHelper.sendCommand(pass)
        KataGoHelper.sendCommand("showboard")
        KataGoHelper.sendCommand("printsgf")
        if gobanState.analysisStatus == .run {
            let config = gameRecord.config
            gobanState.requestAnalysis(config: config)
        } else {
            gobanState.requestingClearAnalysis = true
        }
    }

    func backwardAction() {
        gameRecord.undo()
        KataGoHelper.sendCommand("undo")
        KataGoHelper.sendCommand("showboard")
        if gobanState.analysisStatus == .run {
            let config = gameRecord.config
            gobanState.requestAnalysis(config: config)
        } else {
            gobanState.analysisStatus = .clear
            gobanState.requestingClearAnalysis = true
        }
    }

    func startAnalysisAction() {
        gobanState.analysisStatus = .run
        let config = gameRecord.config
        gobanState.requestAnalysis(config: config)
    }

    func pauseAnalysisAction() {
        gobanState.analysisStatus = .pause
        KataGoHelper.sendCommand("stop")
    }

    func stopAction() {
        gobanState.analysisStatus = .clear
        KataGoHelper.sendCommand("stop")
    }

    func forwardAction() {
        let currentIndex = gameRecord.currentIndex
        let sgfHelper = SgfHelper(sgf: gameRecord.sgf)
        if let nextMove = sgfHelper.getMove(at: currentIndex) {
            if let move = locationToMove(location: nextMove.location) {
                gameRecord.currentIndex = currentIndex + 1
                let nextPlayer = nextMove.player == Player.black ? "b" : "w"
                KataGoHelper.sendCommand("play \(nextPlayer) \(move)")
                player.nextColorForPlayCommand = (nextPlayer == "b") ? .white : .black
            }
        }

        KataGoHelper.sendCommand("showboard")
        if gobanState.analysisStatus == .run {
            let config = gameRecord.config
            gobanState.requestAnalysis(config: config)
        } else {
            gobanState.analysisStatus = .clear
            gobanState.requestingClearAnalysis = true
        }
    }

    func clearBoardAction() {
        gameRecord.currentIndex = 0
        KataGoHelper.sendCommand("clear_board")
        KataGoHelper.sendCommand("showboard")
        if gobanState.analysisStatus == .run {
            let config = gameRecord.config
            gobanState.requestAnalysis(config: config)
        } else {
            gobanState.analysisStatus = .clear
            gobanState.requestingClearAnalysis = true
        }
    }

    func locationToMove(location: Location) -> String? {
        guard !location.pass else { return "pass" }
        let x = location.x
        let y = Int(board.height) - location.y

        guard (1...Int(board.height)).contains(y), (0..<Int(board.width)).contains(x) else { return nil }

        return Coordinate.xLabelMap[x].map { "\($0)\(y)" }
    }
}

struct ToolbarView: View {
    @Environment(\.horizontalSizeClass) var hSizeClass
    @Environment(\.verticalSizeClass) var vSizeClass
    @EnvironmentObject var gobanState: GobanState
    var gameRecord: GameRecord

    var body: some View {
        if hSizeClass == .compact && vSizeClass == .regular {
            HStack {
                ToolbarItems(gameRecord: gameRecord)
            }
        } else {
            VStack {
                ToolbarItems(gameRecord: gameRecord)
            }
            .frame(maxWidth: 80)
        }
    }
}
