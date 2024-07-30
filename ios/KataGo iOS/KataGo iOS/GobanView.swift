//
//  GobanView.swift
//  KataGo iOS
//
//  Created by Chin-Chang Yang on 2023/9/2.
//

import SwiftUI
import KataGoInterface

struct BoardView: View {
    @EnvironmentObject var board: ObservableBoard
    @EnvironmentObject var player: PlayerObject
    @EnvironmentObject var gobanState: GobanState
    var config: Config

    var body: some View {
        GeometryReader { geometry in
            let dimensions = Dimensions(geometry: geometry,
                                        width: board.width,
                                        height: board.height,
                                        showCoordinate: config.showCoordinate)
            ZStack {
                BoardLineView(dimensions: dimensions)

                StoneView(dimensions: dimensions,
                          isClassicStoneStyle: config.isClassicStoneStyle())

                if gobanState.analysisStatus != .clear {
                    AnalysisView(config: config, dimensions: dimensions)
                }

                MoveNumberView(dimensions: dimensions)
                WinrateBarView(dimensions: dimensions)
            }
            .onTapGesture() { location in
                if let move = locationToMove(location: location, dimensions: dimensions) {
                    if player.nextColorForPlayCommand == .black {
                        KataGoHelper.sendCommand("play b \(move)")
                        player.nextColorForPlayCommand = .white
                    } else {
                        KataGoHelper.sendCommand("play w \(move)")
                        player.nextColorForPlayCommand = .black
                    }
                }

                KataGoHelper.sendCommand("showboard")
                KataGoHelper.sendCommand("printsgf")

                if gobanState.analysisStatus != .clear {
                    gobanState.requestAnalysis(config: config)
                } else {
                    gobanState.requestingClearAnalysis = true
                }
            }
        }
        .onAppear() {
            KataGoHelper.sendCommand(config.getKataPlayoutDoublingAdvantageCommand())
            KataGoHelper.sendCommand(config.getKataAnalysisWideRootNoiseCommand())
            KataGoHelper.sendCommand("kata-set-param humanSLProfile \(config.humanSLProfile)")
            KataGoHelper.sendCommand("kata-set-param humanSLRootExploreProbWeightful \(config.humanSLRootExploreProbWeightful)")
            KataGoHelper.sendCommand("showboard")
            if (gobanState.analysisStatus == .run) {
                gobanState.requestAnalysis(config: config)
            }
        }
        .onDisappear() {
            KataGoHelper.sendCommand("stop")
        }
    }

    func locationToMove(location: CGPoint, dimensions: Dimensions) -> String? {
        let calculateCoordinate = { (point: CGFloat, margin: CGFloat, length: CGFloat) -> Int in
            return Int(round((point - margin) / length))
        }

        let y = calculateCoordinate(location.y, dimensions.boardLineStartY, dimensions.squareLength) + 1
        let x = calculateCoordinate(location.x, dimensions.boardLineStartX, dimensions.squareLength)

        guard (1...Int(board.height)).contains(y), (0..<Int(board.width)).contains(x) else { return nil }

        return Coordinate.xLabelMap[x].map { "\($0)\(y)" }
    }
}

struct TopToolbarView: View {
    var config: Config
    @Binding var isCommandPresented: Bool
    @Binding var isConfigPresented: Bool
    @Binding var isBoardSizeChanged: Bool

    var body: some View {
        HStack {
            Button(action: {
                withAnimation {
                    isCommandPresented.toggle()
                    isConfigPresented = false
                }
            }) {
                if isCommandPresented {
                    Image(systemName: "doc.plaintext.fill")
                } else {
                    Image(systemName: "doc.plaintext")
                }
            }

            Button(action: {
                withAnimation {
                    isCommandPresented = false
                    isConfigPresented.toggle()
                }
            }) {
                if isConfigPresented {
                    Image(systemName: "gearshape.fill")
                } else {
                    Image(systemName: "gearshape")
                }
            }
            .onChange(of: isConfigPresented) { _, isConfigPresentedNow in
                if !isConfigPresentedNow && (isBoardSizeChanged) {
                    KataGoHelper.sendCommand(config.getKataBoardSizeCommand())
                    KataGoHelper.sendCommand("printsgf")
                    isBoardSizeChanged = false
                }
            }
        }
    }
}

struct TopToolbarContent: ToolbarContent {
    var config: Config
    @Binding var isCommandPresented: Bool
    @Binding var isConfigPresented: Bool
    @Binding var isBoardSizeChanged: Bool

    var body: some ToolbarContent {
        ToolbarItem {
            TopToolbarView(config: config,
                           isCommandPresented: $isCommandPresented,
                           isConfigPresented: $isConfigPresented,
                           isBoardSizeChanged: $isBoardSizeChanged)
        }
    }
}

struct GobanItems: View {
    var gameRecord: GameRecord
    @State private var isCommandPresented = false
    @State private var isConfigPresented = false
    @State private var isBoardSizeChanged = false

    var body: some View {
        if isCommandPresented {
            CommandView(config: gameRecord.config)
                .toolbar {
                    TopToolbarContent(config: gameRecord.config,
                                      isCommandPresented: $isCommandPresented,
                                      isConfigPresented: $isConfigPresented,
                                      isBoardSizeChanged: $isBoardSizeChanged)
                }
        } else if isConfigPresented {
            ConfigView(config: gameRecord.config, isBoardSizeChanged: $isBoardSizeChanged)
                .toolbar {
                    TopToolbarContent(config: gameRecord.config,
                                      isCommandPresented: $isCommandPresented,
                                      isConfigPresented: $isConfigPresented,
                                      isBoardSizeChanged: $isBoardSizeChanged)
                }
        } else {
            BoardView(config: gameRecord.config)
                .toolbar {
                    TopToolbarContent(config: gameRecord.config,
                                      isCommandPresented: $isCommandPresented,
                                      isConfigPresented: $isConfigPresented,
                                      isBoardSizeChanged: $isBoardSizeChanged)
                }
            ToolbarView(gameRecord: gameRecord)
                .padding()
        }
    }
}

struct GobanView: View {
    @Environment(\.horizontalSizeClass) var hSizeClass
    @Environment(\.verticalSizeClass) var vSizeClass
    var gameRecord: GameRecord?

    var body: some View {
        if let gameRecord {
            if hSizeClass == .compact && vSizeClass == .regular {
                VStack {
                    GobanItems(gameRecord: gameRecord)
                }
            } else {
                HStack {
                    GobanItems(gameRecord: gameRecord)
                }
            }
        } else {
            ContentUnavailableView("Select a game record", systemImage: "sidebar.left")
        }
    }
}
