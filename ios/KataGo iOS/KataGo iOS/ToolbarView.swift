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
                if gobanState.showingAnalysis {
                    KataGoHelper.sendCommand(config.getKataAnalyzeCommand())
                }
            }) {
                Image(systemName: "hand.raised")
            }
            .padding()

            Button(action: {
                KataGoHelper.sendCommand("undo")
                KataGoHelper.sendCommand("showboard")
                if (!gobanState.paused) && gobanState.showingAnalysis {
                    KataGoHelper.sendCommand(config.getKataAnalyzeCommand())
                } else {
                    gobanState.paused = true
                    gobanState.showingAnalysis = false
                }
            }) {
                Image(systemName: "backward.frame")
            }
            .padding()

            if gobanState.paused {
                Button(action: {
                    gobanState.paused = false
                    gobanState.showingAnalysis = true
                    KataGoHelper.sendCommand(config.getKataAnalyzeCommand())
                }) {
                    Image(systemName: "play")
                }
                .padding()
            } else {
                Button(action: {
                    gobanState.paused = true
                    gobanState.showingAnalysis = true
                    KataGoHelper.sendCommand("stop")
                }) {
                    Image(systemName: "pause")
                }
                .padding()
            }

            Button(action: {
                gobanState.paused = true
                gobanState.showingAnalysis = false
            }) {
                Image(systemName: "stop")
            }
            .padding()

            Button(action: {
                let nextColor = (player.nextColorForPlayCommand == .black) ? "b" : "w"
                KataGoHelper.sendCommand("genmove \(nextColor)")
                KataGoHelper.sendCommand("showboard")
                if (!gobanState.paused) && gobanState.showingAnalysis {
                    KataGoHelper.sendCommand(config.getKataAnalyzeCommand())
                } else {
                    gobanState.paused = true
                    gobanState.showingAnalysis = false
                }
            }) {
                Image(systemName: "forward.frame")
            }
            .padding()

            Button(action: {
                KataGoHelper.sendCommand("clear_board")
                KataGoHelper.sendCommand("showboard")
                if (!gobanState.paused) && gobanState.showingAnalysis {
                    KataGoHelper.sendCommand(config.getKataAnalyzeCommand())
                } else {
                    gobanState.paused = true
                    gobanState.showingAnalysis = false
                }
            }) {
                Image(systemName: "clear")
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
