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
    @Binding var paused: Bool
    @Binding var showingAnalysis: Bool

    var body: some View {
        Group {
            Button(action: {
                let nextColor = (player.nextColorForPlayCommand == .black) ? "b" : "w"
                let pass = "play \(nextColor) pass"
                KataGoHelper.sendCommand(pass)
                KataGoHelper.sendCommand("showboard")
                if showingAnalysis {
                    KataGoHelper.sendCommand(config.getKataAnalyzeCommand())
                }
            }) {
                Image(systemName: "hand.raised")
            }
            .padding()

            Button(action: {
                KataGoHelper.sendCommand("undo")
                KataGoHelper.sendCommand("showboard")
                if (!paused) && showingAnalysis {
                    KataGoHelper.sendCommand(config.getKataAnalyzeCommand())
                } else {
                    paused = true
                    showingAnalysis = false
                }
            }) {
                Image(systemName: "backward.frame")
            }
            .padding()

            if paused {
                Button(action: {
                    paused = false
                    showingAnalysis = true
                    KataGoHelper.sendCommand(config.getKataAnalyzeCommand())
                }) {
                    Image(systemName: "play")
                }
                .padding()
            } else {
                Button(action: {
                    paused = true
                    showingAnalysis = true
                    KataGoHelper.sendCommand("stop")
                }) {
                    Image(systemName: "pause")
                }
                .padding()
            }

            Button(action: {
                paused = true
                showingAnalysis = false
            }) {
                Image(systemName: "stop")
            }
            .padding()

            Button(action: {
                let nextColor = (player.nextColorForPlayCommand == .black) ? "b" : "w"
                KataGoHelper.sendCommand("genmove \(nextColor)")
                KataGoHelper.sendCommand("showboard")
                if (!paused) && showingAnalysis {
                    KataGoHelper.sendCommand(config.getKataAnalyzeCommand())
                } else {
                    paused = true
                    showingAnalysis = false
                }
            }) {
                Image(systemName: "forward.frame")
            }
            .padding()

            Button(action: {
                KataGoHelper.sendCommand("clear_board")
                KataGoHelper.sendCommand("showboard")
                if (!paused) && showingAnalysis {
                    KataGoHelper.sendCommand(config.getKataAnalyzeCommand())
                } else {
                    paused = true
                    showingAnalysis = false
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
    @Binding var paused: Bool
    @Binding var showingAnalysis: Bool

    var body: some View {
        if hSizeClass == .compact && vSizeClass == .regular {
            HStack {
                ToolbarItems(paused: $paused, showingAnalysis: $showingAnalysis)
            }
        } else {
            VStack {
                ToolbarItems(paused: $paused, showingAnalysis: $showingAnalysis)
            }
        }
    }
}

struct ToolbarView_Previews: PreviewProvider {
    static let player = PlayerObject()
    static let config = Config()

    static var previews: some View {
        @State var paused = false
        @State var showingAnalysis = true
        ToolbarView(paused: $paused, showingAnalysis: $showingAnalysis)
            .environmentObject(player)
            .environmentObject(config)
    }
}
