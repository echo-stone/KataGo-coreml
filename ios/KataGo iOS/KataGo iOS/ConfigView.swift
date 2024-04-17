//
//  ConfigView.swift
//  KataGo iOS
//
//  Created by Chin-Chang Yang on 2023/9/19.
//

import SwiftUI

struct EditButtonBar: View {
    var body: some View {
        HStack {
            Spacer()
            EditButton()
        }
    }
}

struct ConfigIntItem: View {
    @Environment(\.editMode) private var editMode
    let title: String
    @Binding var value: Int

    var body: some View {
        HStack {
            Text(title)
            Spacer()
            if editMode?.wrappedValue.isEditing == true {
                Stepper(value: $value, in: 1...Int.max) {
                    Text("\(value)")
                }
            } else {
                Text("\(value)")
            }
        }
        .frame(maxWidth: .infinity, alignment: .leading)
    }
}

struct ConfigFloatItem: View {
    @Environment(\.editMode) private var editMode
    let title: String
    @Binding var value: Float
    let step: Float

    var body: some View {
        HStack {
            Text(title)
            Spacer()
            if editMode?.wrappedValue.isEditing == true {
                Stepper(value: $value, in: -.infinity...(.infinity), step: step) {
                    Text("\(value.formatted(.number))")
                }
            } else {
                Text("\(value.formatted(.number))")
            }
        }
        .frame(maxWidth: .infinity, alignment: .leading)
    }
}

struct ConfigTextItem: View {
    @Environment(\.editMode) private var editMode
    let title: String
    let texts: [String]
    @Binding var value: Int

    var body: some View {
        HStack {
            Text(title)
            Spacer()
            if editMode?.wrappedValue.isEditing == true {
                Stepper {
                    Text(texts[value])
                } onIncrement: {
                    value = ((value + 1) < texts.count) ? (value + 1) : 0
                } onDecrement: {
                    value = ((value - 1) >= 0) ? (value - 1) : (texts.count - 1)
                }
            } else {
                Text(texts[value])
            }
        }
        .frame(maxWidth: .infinity, alignment: .leading)
    }
}

struct ConfigItems: View {
    @EnvironmentObject var config: Config
    @State var boardWidth: Int = Config.defaultBoardWidth
    @State var boardHeight: Int = Config.defaultBoardHeight
    @State var rule: Int = Config.defaultRule
    @State var komi: Float = Config.defaultKomi
    @State var playoutDoublingAdvantage: Float = Config.defaultPlayoutDoublingAdvantage
    @State var analysisWideRootNoise: Float = Config.defaultAnalysisWideRootNoise
    @State var maxMessageCharacters: Int = Config.defaultMaxMessageCharacters
    @State var maxAnalysisMoves: Int = Config.defaultMaxAnalysisMoves
    @State var analysisInterval: Int = Config.defaultAnalysisInterval
    @State var maxMessageLines: Int = Config.defaultMaxMessageLines

    var body: some View {
        VStack {
            ConfigIntItem(title: "Board width:", value: $boardWidth)
                .onChange(of: boardWidth) { newValue in
                    config.boardWidth = newValue
                    KataGoHelper.sendCommand(config.getKataBoardSizeCommand())
                }
                .padding(.bottom)

            ConfigIntItem(title: "Board height:", value: $boardHeight)
                .onChange(of: boardHeight) { newValue in
                    config.boardHeight = newValue
                    KataGoHelper.sendCommand(config.getKataBoardSizeCommand())
                }
                .padding(.bottom)

            ConfigTextItem(title: "Rule:", texts: Config.rules, value: $rule)
                .onChange(of: rule) { newValue in
                    config.rule = newValue
                    KataGoHelper.sendCommand(config.getKataRuleCommand())
            }
            .padding(.bottom)

            ConfigFloatItem(title: "Komi:", value: $komi, step: 0.5)
                .onChange(of: komi) { newValue in
                    config.komi = newValue
                    KataGoHelper.sendCommand(config.getKataKomiCommand())
            }
            .padding(.bottom)

            ConfigFloatItem(title: "Playout doubling advantage:", value: $playoutDoublingAdvantage, step: 0.125)
                .onChange(of: playoutDoublingAdvantage) { newValue in
                    config.playoutDoublingAdvantage = newValue
                    KataGoHelper.sendCommand(config.getKataPlayoutDoublingAdvantageCommand())
            }
            .padding(.bottom)

            ConfigFloatItem(title: "Analysis wide root noise:", value: $analysisWideRootNoise, step: 0.0078125)
                .onChange(of: analysisWideRootNoise) { newValue in
                    config.analysisWideRootNoise = newValue
                    KataGoHelper.sendCommand(config.getKataAnalysisWideRootNoiseCommand())
            }
            .padding(.bottom)

            ConfigIntItem(title: "Max message characters:", value: $maxMessageCharacters)
                .onChange(of: maxMessageCharacters) { newValue in
                    config.maxMessageCharacters = newValue
                }
                .padding(.bottom)

            ConfigIntItem(title: "Max analysis moves:", value: $maxAnalysisMoves)
                .onChange(of: maxAnalysisMoves) { newValue in
                    config.maxAnalysisMoves = newValue
                }
                .padding(.bottom)

            ConfigIntItem(title: "Analysis interval (centiseconds):", value: $analysisInterval)
                .onChange(of: analysisInterval) { newValue in
                    config.analysisInterval = newValue
                }
                .padding(.bottom)

            ConfigIntItem(title: "Max message lines:", value: $maxMessageLines)
                .onChange(of: maxMessageLines) { newValue in
                    config.maxMessageLines = newValue
                }
        }
    }
}

struct ConfigView: View {
    var body: some View {
        VStack {
            EditButtonBar()
                .padding()
            ConfigItems()
                .padding()
        }
        .frame(maxHeight: .infinity, alignment: .topLeading)
        .onAppear() {
            KataGoHelper.sendCommand("stop")
        }
    }
}

struct ConfigView_Previews: PreviewProvider {
    static let isEditing = EditMode.inactive
    static let config = Config()
    static var previews: some View {
        ConfigView()
            .environmentObject(config)
    }
}
