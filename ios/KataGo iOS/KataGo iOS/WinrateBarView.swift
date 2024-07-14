//
//  WinrateBarView.swift
//  KataGo iOS
//
//  Created by Chin-Chang Yang on 2024/7/1.
//

import SwiftUI

struct WinrateBarView: View {
    @EnvironmentObject var winrate: Winrate
    let dimensions: Dimensions

    var body: some View {
        let width = dimensions.squareLength / 2 - dimensions.squareLength / 8
        let positionXBegin = dimensions.boardLineStartX - width - dimensions.squareLength / 2 - dimensions.squareLength / 4 + dimensions.squareLength / 8 + dimensions.squareLength / 16
        let positionXEnd = positionXBegin + width
        let positionX = (positionXBegin + positionXEnd) / 2
        let barHeight = dimensions.gobanHeight
        let whiteBarHeight = barHeight * CGFloat(winrate.white)
        let blackBarHeight = barHeight - whiteBarHeight
        let whiteBarPositionYBegin = dimensions.boardLineStartY - dimensions.squareLength / 2 - dimensions.squareLength / 4
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
}
