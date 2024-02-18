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

struct ConfigItem: View {
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

struct ConfigItems: View {
    @EnvironmentObject var config: Config
    @State var maxMessageCharacters: Int = Config.defaultMaxMessageCharacters
    @State var maxAnalysisMoves: Int = Config.defaultMaxAnalysisMoves
    @State var analysisInterval: Int = Config.defaultAnalysisInterval
    @State var maxMessageLines: Int = Config.defaultMaxMessageLines

    var body: some View {
        VStack {
            ConfigItem(title: "Max message characters:", value: $maxMessageCharacters)
                .onChange(of: maxMessageCharacters) { newValue in
                    config.maxMessageCharacters = newValue
                }
                .padding(.bottom)

            ConfigItem(title: "Max analysis moves:", value: $maxAnalysisMoves)
                .onChange(of: maxAnalysisMoves) { newValue in
                    config.maxAnalysisMoves = newValue
                }
                .padding(.bottom)

            ConfigItem(title: "Analysis interval (centiseconds):", value: $analysisInterval)
                .onChange(of: analysisInterval) { newValue in
                    config.analysisInterval = newValue
                }
                .padding(.bottom)

            ConfigItem(title: "Max message lines:", value: $maxMessageLines)
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
