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
    @EnvironmentObject var config: Config
    @EnvironmentObject var gobanState: GobanState
    @EnvironmentObject var board: ObservableBoard
    var gameRecord: GameRecord?

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

            if gobanState.paused {
                Button(action: startAnalysisAction) {
                    Image(systemName: "sparkle")
                        .resizable()
                        .scaledToFit()
                }
                .padding()
            } else {
                Button(action: pauseAnalysisAction) {
                    Image(systemName: "sparkle")
                        .resizable()
                        .scaledToFit()
                        .symbolEffect(.variableColor.iterative.reversing, isActive: true)
                }
                .padding()
            }

            Button(action: stopAction) {
                Image(systemName: "stop")
                    .resizable()
                    .scaledToFit()
            }
            .padding()

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
        if (!gobanState.paused) && gobanState.showingAnalysis {
            gobanState.requestAnalysis(config: config)
        } else {
            gobanState.requestingClearAnalysis = true
        }
    }

    func backwardAction() {
        gameRecord?.undo()
        KataGoHelper.sendCommand("undo")
        KataGoHelper.sendCommand("showboard")
        if (!gobanState.paused) && gobanState.showingAnalysis {
            gobanState.requestAnalysis(config: config)
        } else {
            gobanState.paused = true
            gobanState.showingAnalysis = false
            gobanState.requestingClearAnalysis = true
        }
    }

    func startAnalysisAction() {
        gobanState.paused = false
        gobanState.showingAnalysis = true
        gobanState.requestAnalysis(config: config)
    }

    func pauseAnalysisAction() {
        gobanState.paused = true
        gobanState.showingAnalysis = true
        KataGoHelper.sendCommand("stop")
    }

    func stopAction() {
        gobanState.paused = true
        gobanState.showingAnalysis = false
        KataGoHelper.sendCommand("stop")
    }

    func forwardAction() {
        if let gameRecord {
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
        }

        KataGoHelper.sendCommand("showboard")
        if (!gobanState.paused) && gobanState.showingAnalysis {
            gobanState.requestAnalysis(config: config)
        } else {
            gobanState.paused = true
            gobanState.showingAnalysis = false
            gobanState.requestingClearAnalysis = true
        }
    }

    func clearBoardAction() {
        gameRecord?.currentIndex = 0
        KataGoHelper.sendCommand("clear_board")
        KataGoHelper.sendCommand("showboard")
        if (!gobanState.paused) && gobanState.showingAnalysis {
            gobanState.requestAnalysis(config: config)
        } else {
            gobanState.paused = true
            gobanState.showingAnalysis = false
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
    var gameRecord: GameRecord?

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

struct ToolbarView_Previews: PreviewProvider {
    static let player = PlayerObject()
    static let config = Config()
    static let gobanState = GobanState()

    static var previews: some View {
        ToolbarView()
            .environmentObject(player)
            .environmentObject(config)
            .environmentObject(gobanState)
    }
}
