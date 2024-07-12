//
//  MoveNumberView.swift
//  KataGo iOS
//
//  Created by Chin-Chang Yang on 2024/6/15.
//

import SwiftUI

struct MoveNumberView: View {
    @EnvironmentObject var stones: Stones
    @EnvironmentObject var board: ObservableBoard
    let dimensions: Dimensions

    var body: some View {
        drawMoveOrder(dimensions: dimensions)
    }

    private func drawMoveOrder(dimensions: Dimensions) -> some View {
        Group {
            ForEach(stones.moveOrder.keys.sorted(), id: \.self) { key in
                if let point = stones.moveOrder[key] {
                    let color: Color = stones.blackPoints.contains { blackPoint in
                        point == blackPoint
                    } ? .white : .black
                    Text(String(key))
                        .foregroundStyle(color)
                        .font(.system(size: 500, design: .monospaced))
                        .minimumScaleFactor(0.01)
                        .bold()
                        .frame(width: dimensions.squareLength, height: dimensions.squareLength)
                        .position(x: dimensions.marginWidth + CGFloat(point.x) * dimensions.squareLength,
                                  y: dimensions.marginHeight + CGFloat(point.y) * dimensions.squareLength)
                }
            }
        }
    }
}

struct MoveNumberView_Previews: PreviewProvider {
    static let stones = Stones()
    static let board = ObservableBoard()
    static var previews: some View {
        ZStack {
            Rectangle()
                .foregroundColor(.brown)

            GeometryReader { geometry in
                let dimensions = Dimensions(geometry: geometry,
                                            width: board.width,
                                            height: board.height)
                MoveNumberView(dimensions: dimensions)
            }
            .environmentObject(stones)
            .environmentObject(board)
            .onAppear() {
                MoveNumberView_Previews.board.width = 2
                MoveNumberView_Previews.board.height = 2
                MoveNumberView_Previews.stones.blackPoints = [BoardPoint(x: 0, y: 0), BoardPoint(x: 1, y: 1)]
                MoveNumberView_Previews.stones.whitePoints = [BoardPoint(x: 0, y: 1), BoardPoint(x: 1, y: 0)]
                MoveNumberView_Previews.stones.moveOrder = ["1": BoardPoint(x: 0, y: 0),
                                                       "2": BoardPoint(x: 0, y: 1),
                                                       "3": BoardPoint(x: 1, y: 1),
                                                       "4": BoardPoint(x: 1, y: 0)]
            }
        }
    }
}
