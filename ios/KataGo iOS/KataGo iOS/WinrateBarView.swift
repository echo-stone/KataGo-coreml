//
//  WinrateBarView.swift
//  KataGo iOS
//
//  Created by Chin-Chang Yang on 2024/7/1.
//

import SwiftUI

struct WinrateBarView: View {
    @EnvironmentObject var analysis: Analysis
    @EnvironmentObject var board: ObservableBoard
    let dimensions: Dimensions

    var body: some View {
        let positionX = dimensions.marginWidth - dimensions.squareLength + dimensions.squareLength / 4 + dimensions.squareLength / 8
        let width = dimensions.squareLength / 2 - dimensions.squareLength / 8
        let barHeight = (dimensions.boardHeight + dimensions.squareLength / 2)
        let whiteBarHeight = barHeight * CGFloat(getWhiteWinrate())
        let blackBarHeight = barHeight - whiteBarHeight
        let whiteBarPositionYBegin = dimensions.marginHeight - dimensions.squareLength / 2 - dimensions.squareLength / 4
        let whiteBarPositionYEnd = whiteBarPositionYBegin + whiteBarHeight
        let blackBarPositionYBegin = whiteBarPositionYEnd
        let blackBarPositionYEnd = blackBarPositionYBegin + blackBarHeight
        let whiteBarPositionY = (whiteBarPositionYBegin + whiteBarPositionYEnd) / 2
        let blackBarPositionY = (blackBarPositionYBegin + blackBarPositionYEnd) / 2

        Group {
            Rectangle()
                .frame(width: width, height: whiteBarHeight)
                .foregroundColor(.white)
                .position(x: positionX, y: whiteBarPositionY)

            Rectangle()
                .frame(width: width, height: blackBarHeight)
                .foregroundColor(.black)
                .position(x: positionX, y: blackBarPositionY)
        }
    }

    func getBlackWinrate(_ info: [BoardPoint: AnalysisInfo]) -> Float {
        let points = info.keys.sorted()

        let visits = points.map { point in
            info[point]?.visits ?? 0
        }

        let sumVisits = visits.reduce(1, +)

        let weightedWinrates = points.map() { point in
            let winrate = info[point]?.winrate ?? 0.5
            let visit = info[point]?.visits ?? 0
            return winrate * Float(visit) / Float(sumVisits)
        }

        let winrate = weightedWinrates.reduce(0, +)
        let blackWinrate = (analysis.nextColorForAnalysis == .black) ? winrate : (1 - winrate)

        return blackWinrate
    }

    func getBlackWinrate() -> Float {
        return analysis.info.isEmpty ? 0.5 : getBlackWinrate(analysis.info)
    }

    func getWhiteWinrate() -> Float {
        return 1 - getBlackWinrate()
    }
}
