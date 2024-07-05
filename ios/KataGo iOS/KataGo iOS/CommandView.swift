//
//  CommandView.swift
//  KataGo iOS
//
//  Created by Chin-Chang Yang on 2023/9/2.
//

import SwiftUI
import KataGoHelper

struct CommandView: View {
    @EnvironmentObject var messagesObject: MessagesObject
    @EnvironmentObject var stones: Stones
    @EnvironmentObject var config: Config
    @State private var command = ""

    var body: some View {
        VStack {
            ScrollViewReader { scrollView in
                ScrollView(.vertical) {
                    // Vertically show each KataGo message
                    LazyVStack {
                        ForEach(messagesObject.messages) { message in
                            Text(message.text)
                                .font(.body.monospaced())
                                .id(message.id)
                                .textSelection(.enabled)
                                .frame(maxWidth: .infinity, alignment: .leading)
                        }
                    }
                    .onChange(of: messagesObject.messages) { _, newValue in
                        // Scroll to the last message
                        scrollView.scrollTo(newValue.last?.id)
                    }
                }
            }

            HStack {
                TextField("Enter your GTP command (list_commands)", text: $command)
                    .disableAutocorrection(true)
                    .textInputAutocapitalization(.never)
                    .onSubmit {
                        messagesObject.messages.append(Message(text: command,  maxLength: config.maxMessageCharacters))
                        KataGoHelper.sendCommand(command)
                        command = ""
                    }
                Button(action: {
                    messagesObject.messages.append(Message(text: command, maxLength: config.maxMessageCharacters))
                    KataGoHelper.sendCommand(command)
                    command = ""
                }) {
                    Image(systemName: "paperplane")
                }
            }
            .padding()
        }
        .padding()
        .onAppear() {
            KataGoHelper.sendCommand("stop")
        }
    }
}

struct CommandView_Previews: PreviewProvider {
    static let messageObject = MessagesObject()
    static let config = Config()

    static var previews: some View {
        CommandView()
            .environmentObject(messageObject)
            .environmentObject(config)
    }
}
