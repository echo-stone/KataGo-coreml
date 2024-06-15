//
//  GobanView.swift
//  KataGo iOS
//
//  Created by Chin-Chang Yang on 2023/9/2.
//

import SwiftUI

class GobanState: ObservableObject {
    @Published var paused = false
    @Published var showingAnalysis = true
}

struct GobanItems: View {
    @EnvironmentObject var stones: Stones
    @EnvironmentObject var board: ObservableBoard
    @EnvironmentObject var player: PlayerObject
    @EnvironmentObject var analysis: Analysis
    @EnvironmentObject var config: Config
    @StateObject var gobanState = GobanState()
    let texture = WoodImage.createTexture()

    var body: some View {
        Group {
            GeometryReader { geometry in
                let dimensions = Dimensions(geometry: geometry, board: board)
                ZStack {
                    BoardLineView(dimensions: dimensions, boardWidth: board.width, boardHeight: board.height)
                    StoneView(geometry: geometry)
                    if gobanState.showingAnalysis {
                        AnalysisView(geometry: geometry)
                    }

                    MoveNumberView(geometry: geometry)
                }
                .onTapGesture(coordinateSpace: .local) { location in
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
                    if gobanState.showingAnalysis {
                        gobanState.paused = false
                        KataGoHelper.sendCommand(config.getKataAnalyzeCommand())

                    }
                }
            }
            .onAppear() {
                KataGoHelper.sendCommand("showboard")
                if (!gobanState.paused) && gobanState.showingAnalysis {
                    KataGoHelper.sendCommand(config.getKataAnalyzeCommand())
                }
            }
            .onChange(of: config.maxAnalysisMoves) { _, _ in
                if (!gobanState.paused) && gobanState.showingAnalysis {
                    KataGoHelper.sendCommand(config.getKataAnalyzeCommand())
                }
            }

            ToolbarView()
                .padding()
        }
        .environmentObject(gobanState)
    }

    func locationToMove(location: CGPoint, dimensions: Dimensions) -> String? {
        let calculateCoordinate = { (point: CGFloat, margin: CGFloat, length: CGFloat) -> Int in
            return Int(round((point - margin) / length))
        }

        let y = calculateCoordinate(location.y, dimensions.marginHeight, dimensions.squareLength) + 1
        let x = calculateCoordinate(location.x, dimensions.marginWidth, dimensions.squareLength)

        guard (1...Int(board.height)).contains(y), (0..<Int(board.width)).contains(x) else { return nil }

        let letterMap: [Int: String] = [
            0: "A", 1: "B", 2: "C", 3: "D", 4: "E",
            5: "F", 6: "G", 7: "H", 8: "J", 9: "K",
            10: "L", 11: "M", 12: "N", 13: "O", 14: "P",
            15: "Q", 16: "R", 17: "S", 18: "T", 19: "U",
            20: "V", 21: "W", 22: "X", 23: "Y", 24: "Z",
            25: "AA", 26: "AB", 27: "AC", 28: "AD"
        ]

        return letterMap[x].map { "\($0)\(y)" }
    }
}

struct GobanView: View {
    @Environment(\.horizontalSizeClass) var hSizeClass
    @Environment(\.verticalSizeClass) var vSizeClass

    var body: some View {
        if hSizeClass == .compact && vSizeClass == .regular {
            VStack {
                GobanItems()
            }
        } else {
            HStack {
                GobanItems()
            }
        }
    }
}

struct GobanView_Previews: PreviewProvider {
    static let stones = Stones()
    static let board = ObservableBoard()
    static let analysis = Analysis()
    static let player = PlayerObject()
    static let config = Config()

    static var previews: some View {
        GobanView()
            .environmentObject(stones)
            .environmentObject(board)
            .environmentObject(analysis)
            .environmentObject(player)
            .environmentObject(config)
            .onAppear() {
                GobanView_Previews.board.width = 3
                GobanView_Previews.board.height = 3
                GobanView_Previews.stones.blackPoints = [BoardPoint(x: 1, y: 1), BoardPoint(x: 0, y: 1)]
                GobanView_Previews.stones.whitePoints = [BoardPoint(x: 0, y: 0), BoardPoint(x: 1, y: 0)]
                GobanView_Previews.analysis.data = [["move": "C1", "winrate": "0.54321012345", "visits": "1234567890", "scoreLead": "8.987654321"]]
                GobanView_Previews.stones.moveOrder = ["1": BoardPoint(x: 0, y: 1),
                                                       "2": BoardPoint(x: 0, y: 0),
                                                       "3": BoardPoint(x: 1, y: 1),
                                                       "4": BoardPoint(x: 1, y: 0)]
            }
    }
}
