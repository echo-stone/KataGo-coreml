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
    @EnvironmentObject var config: Config
    @EnvironmentObject var gobanState: GobanState

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

                if gobanState.showingAnalysis {
                    AnalysisView(dimensions: dimensions)
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

                if gobanState.showingAnalysis {
                    gobanState.paused = false
                    gobanState.requestAnalysis(config: config)
                } else {
                    gobanState.requestingClearAnalysis = true
                }
            }
        }
        .onAppear() {
            KataGoHelper.sendCommand("showboard")
            if (!gobanState.paused && gobanState.showingAnalysis) {
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
    @Binding var isCommandPresented: Bool
    @Binding var isConfigPresented: Bool
    @EnvironmentObject var config: Config

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
                if !isConfigPresentedNow && (config.isBoardSizeChanged) {
                    KataGoHelper.sendCommand(config.getKataBoardSizeCommand())
                    KataGoHelper.sendCommand("printsgf")
                    config.isBoardSizeChanged = false
                }
            }
        }
    }
}

struct TopToolbarContent: ToolbarContent {
    @Binding var isCommandPresented: Bool
    @Binding var isConfigPresented: Bool

    var body: some ToolbarContent {
        ToolbarItem {
            TopToolbarView(isCommandPresented: $isCommandPresented,
                           isConfigPresented: $isConfigPresented)
        }
    }
}

struct GobanItems: View {
    var gameRecord: GameRecord?
    @State var isCommandPresented = false
    @State var isConfigPresented = false

    var body: some View {
        if isCommandPresented {
            CommandView()
                .toolbar {
                    TopToolbarContent(isCommandPresented: $isCommandPresented,
                                      isConfigPresented: $isConfigPresented)
                }
        } else if isConfigPresented {
            ConfigView()
                .toolbar {
                    TopToolbarContent(isCommandPresented: $isCommandPresented,
                                      isConfigPresented: $isConfigPresented)
                }
        } else {
            BoardView()
                .toolbar {
                    TopToolbarContent(isCommandPresented: $isCommandPresented,
                                      isConfigPresented: $isConfigPresented)
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

#Preview {
    let stones = Stones()
    let board = ObservableBoard()
    let analysis = Analysis()
    let player = PlayerObject()
    let config = Config()
    let gobanState = GobanState()
    let winrate = Winrate()

    return GobanView()
        .environmentObject(stones)
        .environmentObject(board)
        .environmentObject(analysis)
        .environmentObject(player)
        .environmentObject(config)
        .environmentObject(gobanState)
        .environmentObject(winrate)
        .onAppear() {
            board.width = 3
            board.height = 3
            stones.blackPoints = [BoardPoint(x: 1, y: 1), BoardPoint(x: 0, y: 1)]
            stones.whitePoints = [BoardPoint(x: 0, y: 0), BoardPoint(x: 1, y: 0)]
            analysis.info = [
                BoardPoint(x: 2, y: 0): AnalysisInfo(visits: 1234567890, winrate: 0.789012345, scoreLead: 8.987654321, utilityLcb: -0.123456789)
            ]
            stones.moveOrder = ["1": BoardPoint(x: 0, y: 1),
                                "2": BoardPoint(x: 0, y: 0),
                                "3": BoardPoint(x: 1, y: 1),
                                "4": BoardPoint(x: 1, y: 0)]
            winrate.black = 0.789012345
        }
}

#Preview {
    let stones = Stones()
    let board = ObservableBoard()
    let analysis = Analysis()
    let player = PlayerObject()
    let config = Config()
    let gobanState = GobanState()
    let winrate = Winrate()

    return GobanView()
        .environmentObject(stones)
        .environmentObject(board)
        .environmentObject(analysis)
        .environmentObject(player)
        .environmentObject(config)
        .environmentObject(gobanState)
        .environmentObject(winrate)
        .onAppear() {
            board.width = 3
            board.height = 3
            stones.blackPoints = [BoardPoint(x: 1, y: 1), BoardPoint(x: 0, y: 1)]
            stones.whitePoints = [BoardPoint(x: 0, y: 0), BoardPoint(x: 1, y: 0)]
            analysis.info = [
                BoardPoint(x: 2, y: 0): AnalysisInfo(visits: 1234567890, winrate: 0.789012345, scoreLead: 8.987654321, utilityLcb: -0.123456789)
            ]
            stones.moveOrder = ["1": BoardPoint(x: 0, y: 1),
                                "2": BoardPoint(x: 0, y: 0),
                                "3": BoardPoint(x: 1, y: 1),
                                "4": BoardPoint(x: 1, y: 0)]
            winrate.black = 0.789012345
            config.showCoordinate = true
            config.stoneStyle = 1
        }
}
