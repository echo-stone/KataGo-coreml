//
//  GobanView.swift
//  KataGo iOS
//
//  Created by Chin-Chang Yang on 2023/9/2.
//

import SwiftUI

struct Dimensions {
    let squareLength: CGFloat
    let boardWidth: CGFloat
    let boardHeight: CGFloat
    let marginWidth: CGFloat
    let marginHeight: CGFloat

    init(geometry: GeometryProxy, width: CGFloat, height: CGFloat) {
        let totalWidth = geometry.size.width
        let totalHeight = geometry.size.height
        let totalLength = min(totalWidth, totalHeight)
        let boardSpace: CGFloat = totalLength * 0.05
        let squareWidth = (totalWidth - boardSpace) / (width + 1)
        let squareHeight = (totalHeight - boardSpace) / (height + 1)
        squareLength = min(squareWidth, squareHeight)
        boardWidth = width * squareLength
        boardHeight = height * squareLength
        marginWidth = (totalWidth - boardWidth + squareLength) / 2
        marginHeight = (totalHeight - boardHeight + squareLength) / 2
    }
}

struct GobanView: View {
    @EnvironmentObject var stones: Stones
    @EnvironmentObject var board: Board
    @EnvironmentObject var nextPlayer: PlayerObject
    @EnvironmentObject var analysis: Analysis
    let texture = WoodImage.createTexture()

    var body: some View {
        VStack {
            GeometryReader { geometry in
                let dimensions = Dimensions(geometry: geometry, width: board.width, height: board.height)
                ZStack {
                    drawBoardBackground(texture: texture, dimensions: dimensions)
                    drawLines(dimensions: dimensions)
                    drawStarPoints(dimensions: dimensions)
                    StoneView(dimensions: dimensions)
                    AnalysisView(dimensions: dimensions)
                }
            }
            .gesture(TapGesture().onEnded() { _ in
                if nextPlayer.color == .black {
                    KataGoHelper.sendCommand("genmove b")
                    nextPlayer.color = .white
                } else {
                    KataGoHelper.sendCommand("genmove w")
                    nextPlayer.color = .black
                }

                KataGoHelper.sendCommand("showboard")
                KataGoHelper.sendCommand("kata-analyze interval 10")
            })
        }
        .onAppear() {
            KataGoHelper.sendCommand("showboard")
            KataGoHelper.sendCommand("kata-analyze interval 10")
        }
    }

    private func drawBoardBackground(texture: UIImage, dimensions: Dimensions) -> some View {
        Group {
            Image(uiImage: texture)
                .resizable()
                .frame(width: (dimensions.boardWidth + dimensions.squareLength / 2), height: dimensions.boardHeight + (dimensions.squareLength / 2))
        }
    }

    private func drawLines(dimensions: Dimensions) -> some View {
        Group {
            ForEach(0..<Int(board.height), id: \.self) { i in
                horizontalLine(i: i, dimensions: dimensions)
            }
            ForEach(0..<Int(board.width), id: \.self) { i in
                verticalLine(i: i, dimensions: dimensions)
            }
        }
    }

    private func horizontalLine(i: Int, dimensions: Dimensions) -> some View {
        Path { path in
            path.move(to: CGPoint(x: dimensions.marginWidth, y: dimensions.marginHeight + CGFloat(i) * dimensions.squareLength))
            path.addLine(to: CGPoint(x: dimensions.marginWidth + dimensions.boardWidth - dimensions.squareLength, y: dimensions.marginHeight + CGFloat(i) * dimensions.squareLength))
        }
        .stroke(Color.black)
    }

    private func verticalLine(i: Int, dimensions: Dimensions) -> some View {
        Path { path in
            path.move(to: CGPoint(x: dimensions.marginWidth + CGFloat(i) * dimensions.squareLength, y: dimensions.marginHeight))
            path.addLine(to: CGPoint(x: dimensions.marginWidth + CGFloat(i) * dimensions.squareLength, y: dimensions.marginHeight + dimensions.boardHeight - dimensions.squareLength))
        }
        .stroke(Color.black)
    }

    private func drawStarPoint(x: Int, y: Int, dimensions: Dimensions) -> some View {
        // Big black dot
        Circle()
            .frame(width: dimensions.squareLength / 4, height: dimensions.squareLength / 4)
            .foregroundColor(Color.black)
            .position(x: dimensions.marginWidth + CGFloat(x) * dimensions.squareLength,
                      y: dimensions.marginHeight + CGFloat(y) * dimensions.squareLength)
    }

    private func drawStarPointsForSize(points: [BoardPoint], dimensions: Dimensions) -> some View {
        ForEach(points, id: \.self) { point in
            drawStarPoint(x: point.x, y: point.y, dimensions: dimensions)
        }
    }

    private func drawStarPoints(dimensions: Dimensions) -> some View {
        Group {
            if board.width == 19 && board.height == 19 {
                // Draw star points for 19x19 board
                drawStarPointsForSize(points: [BoardPoint(x: 3, y: 3), BoardPoint(x: 3, y: 9), BoardPoint(x: 3, y: 15), BoardPoint(x: 9, y: 3), BoardPoint(x: 9, y: 9), BoardPoint(x: 9, y: 15), BoardPoint(x: 15, y: 3), BoardPoint(x: 15, y: 9), BoardPoint(x: 15, y: 15)], dimensions: dimensions)
            } else if board.width == 13 && board.height == 13 {
                // Draw star points for 13x13 board
                drawStarPointsForSize(points: [BoardPoint(x: 6, y: 6), BoardPoint(x: 3, y: 3), BoardPoint(x: 3, y: 9), BoardPoint(x: 9, y: 3), BoardPoint(x: 9, y: 9)], dimensions: dimensions)
            } else if board.width == 9 && board.height == 9 {
                // Draw star points for 9x9 board
                drawStarPointsForSize(points: [BoardPoint(x: 4, y: 4), BoardPoint(x: 2, y: 2), BoardPoint(x: 2, y: 6), BoardPoint(x: 6, y: 2), BoardPoint(x: 6, y: 6)], dimensions: dimensions)
            }
        }
    }
}

struct GobanView_Previews: PreviewProvider {
    static let stones = Stones()
    static let board = Board()
    static let analysis = Analysis()

    static var previews: some View {
        GobanView()
            .environmentObject(stones)
            .environmentObject(board)
            .environmentObject(analysis)
            .onAppear() {
                GobanView_Previews.stones.blackPoints = [BoardPoint(x: 15, y: 3), BoardPoint(x: 13, y: 2), BoardPoint(x: 9, y: 3), BoardPoint(x: 3, y: 3)]
                GobanView_Previews.stones.whitePoints = [BoardPoint(x: 3, y: 15)]
                GobanView_Previews.analysis.data = [["move": "Q16", "winrate": "0.54321012345"]]
            }
    }
}
