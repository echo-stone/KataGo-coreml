//
//  ToolbarView.swift
//  KataGo iOS
//
//  Created by Chin-Chang Yang on 2023/10/1.
//

import SwiftUI

struct ToolbarItems: View {
    @EnvironmentObject var player: PlayerObject
    @EnvironmentObject var config: Config
    @EnvironmentObject var gobanState: GobanState

    var body: some View {
        Group {
            Button(action: {
                let nextColor = (player.nextColorForPlayCommand == .black) ? "b" : "w"
                let pass = "play \(nextColor) pass"
                KataGoHelper.sendCommand(pass)
                KataGoHelper.sendCommand("showboard")
                if (!gobanState.paused) && gobanState.showingAnalysis {
                    KataGoHelper.sendCommand(config.getKataFastAnalyzeCommand())
                    KataGoHelper.sendCommand(config.getKataAnalyzeCommand())
                }
            }) {
                Image(systemName: "hand.raised")
                    .resizable()
                    .scaledToFit()
            }
            .padding()

            Button(action: {
                KataGoHelper.sendCommand("undo")
                KataGoHelper.sendCommand("showboard")
                if (!gobanState.paused) && gobanState.showingAnalysis {
                    KataGoHelper.sendCommand(config.getKataFastAnalyzeCommand())
                    KataGoHelper.sendCommand(config.getKataAnalyzeCommand())
                } else {
                    gobanState.paused = true
                    gobanState.showingAnalysis = false
                }
            }) {
                Image(systemName: "backward.frame")
                    .resizable()
                    .scaledToFit()
            }
            .padding()

            if gobanState.paused {
                Button(action: {
                    gobanState.paused = false
                    gobanState.showingAnalysis = true
                    KataGoHelper.sendCommand(config.getKataFastAnalyzeCommand())
                    KataGoHelper.sendCommand(config.getKataAnalyzeCommand())
                }) {
                    Image(systemName: "sparkle")
                        .resizable()
                        .scaledToFit()
                }
                .padding()
            } else {
                Button(action: {
                    gobanState.paused = true
                    gobanState.showingAnalysis = true
                    KataGoHelper.sendCommand("stop")
                }) {
                    Image(systemName: "sparkle")
                        .resizable()
                        .scaledToFit()
                        .symbolEffect(.variableColor.iterative.reversing, isActive: true)
                }
                .padding()
            }

            Button(action: {
                gobanState.paused = true
                gobanState.showingAnalysis = false
                KataGoHelper.sendCommand("stop")
            }) {
                Image(systemName: "stop")
                    .resizable()
                    .scaledToFit()
            }
            .padding()

            Button(action: {
                let nextColor = (player.nextColorForPlayCommand == .black) ? "b" : "w"
                KataGoHelper.sendCommand("genmove \(nextColor)")
                KataGoHelper.sendCommand("showboard")
                if (!gobanState.paused) && gobanState.showingAnalysis {
                    KataGoHelper.sendCommand(config.getKataFastAnalyzeCommand())
                    KataGoHelper.sendCommand(config.getKataAnalyzeCommand())
                } else {
                    gobanState.paused = true
                    gobanState.showingAnalysis = false
                }
            }) {
                Image(systemName: "forward.frame")
                    .resizable()
                    .scaledToFit()
            }
            .padding()

            Button(action: {
                KataGoHelper.sendCommand("clear_board")
                KataGoHelper.sendCommand("showboard")
                if (!gobanState.paused) && gobanState.showingAnalysis {
                    KataGoHelper.sendCommand(config.getKataFastAnalyzeCommand())
                    KataGoHelper.sendCommand(config.getKataAnalyzeCommand())
                } else {
                    gobanState.paused = true
                    gobanState.showingAnalysis = false
                }
            }) {
                Image(systemName: "clear")
                    .resizable()
                    .scaledToFit()
            }
            .padding()
        }
    }
}

struct ToolbarView: View {
    @Environment(\.horizontalSizeClass) var hSizeClass
    @Environment(\.verticalSizeClass) var vSizeClass
    @EnvironmentObject var gobanState: GobanState

    var body: some View {
        if hSizeClass == .compact && vSizeClass == .regular {
            HStack {
                ToolbarItems()
            }
        } else {
            VStack {
                ToolbarItems()
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
