//
//  ContentView.swift
//  KataGo iOS
//
//  Created by Chin-Chang Yang on 2023/7/2.
//

import SwiftUI

enum SidebarItem: Hashable {
    case goban, command, config
}

struct DetailView: View {
    var selectedItem: SidebarItem?

    var body: some View {
        Group {
            switch selectedItem {
            case .command:
                CommandView()
                    .navigationTitle("Command")
            case .config:
                ConfigView()
                    .navigationTitle("Config")
            default:
                GobanView()
            }
        }
    }
}

struct ContentView: View {
    @StateObject var stones = Stones()
    @StateObject var messagesObject = MessagesObject()
    @StateObject var board = ObservableBoard()
    @StateObject var player = PlayerObject()
    @StateObject var analysis = Analysis()
    @StateObject var config = Config()
    @State private var isShowingBoard = false
    @State private var boardText: [String] = []
    @State var isEditing = EditMode.inactive
    @State private var selectedItem: SidebarItem? = .goban

    init() {
        // Start a thread to run KataGo GTP
        Thread {
            KataGoHelper.runGtp()
        }.start()
    }

    var body: some View {
        NavigationSplitView {
            List(selection: $selectedItem) {
                NavigationLink(value: SidebarItem.goban) {
                    Label("Goban", systemImage: "circle")
                }

                NavigationLink(value: SidebarItem.command) {
                    Label("Command", systemImage: "text.alignleft")
                }

                NavigationLink(value: SidebarItem.config) {
                    Label("Config", systemImage: "slider.horizontal.3")
                }
            }
            .navigationTitle("Menu")
        } detail: {
            DetailView(selectedItem: selectedItem)
        }
        .environmentObject(stones)
        .environmentObject(messagesObject)
        .environmentObject(board)
        .environmentObject(player)
        .environmentObject(analysis)
        .environmentObject(config)
        .environment(\.editMode, $isEditing)
        .onAppear() {
            // Get messages from KataGo and append to the list of messages
            createMessageTask()
        }
    }

    /// Create message task
    private func createMessageTask() {
        Task {
            messagesObject.messages.append(Message(text: "Initializing...", maxLength: config.maxMessageCharacters))
            KataGoHelper.sendCommand(config.getKataBoardSizeCommand())
            KataGoHelper.sendCommand(config.getKataRuleCommand())
            KataGoHelper.sendCommand(config.getKataKomiCommand())
            // Disable friendly pass to avoid a memory shortage problem
            KataGoHelper.sendCommand("kata-set-rule friendlyPassOk false")
            KataGoHelper.sendCommand(config.getKataPlayoutDoublingAdvantageCommand())
            KataGoHelper.sendCommand(config.getKataAnalysisWideRootNoiseCommand())
            KataGoHelper.sendCommand("showboard")
            KataGoHelper.sendCommand(config.getKataAnalyzeCommand())

            while true {
                let line = await Task.detached {
                    // Get a message line from KataGo
                    return KataGoHelper.getMessageLine()
                }.value

                // Create a message with the line
                let message = Message(text: line, maxLength: config.maxMessageCharacters)

                // Append the message to the list of messages
                messagesObject.messages.append(message)

                // Collect board information
                maybeCollectBoard(message: line)

                // Collect analysis information
                maybeCollectAnalysis(message: line)

                // Remove when there are too many messages
                while messagesObject.messages.count > config.maxMessageLines {
                    messagesObject.messages.removeFirst()
                }
            }
        }
    }

    func maybeCollectBoard(message: String) {
        if isShowingBoard {
            if message.prefix("Next player".count) == "Next player" {
                isShowingBoard = false
                (stones.blackPoints, stones.whitePoints, board.width, board.height, stones.moveOrder) = parseBoardPoints(board: boardText)
                if message.prefix("Next player: Black".count) == "Next player: Black" {
                    player.nextColorForPlayCommand = .black
                    player.nextColorFromShowBoard = .black
                } else {
                    player.nextColorForPlayCommand = .white
                    player.nextColorFromShowBoard = .white
                }
            } else {
                boardText.append(message)
            }
        } else {
            if message.prefix("= MoveNum".count) == "= MoveNum" {
                boardText = []
                isShowingBoard = true
            }
        }
    }

    func parseBoardPoints(board: [String]) -> ([BoardPoint], [BoardPoint], CGFloat, CGFloat, [Character: BoardPoint]) {
        var blackStones: [BoardPoint] = []
        var whiteStones: [BoardPoint] = []

        let height = CGFloat(board.count - 1)  // Subtracting 1 to exclude the header
        let width = CGFloat((board.last?.dropFirst(2).count ?? 0) / 2)  // Drop the first 2 characters for the y-coordinate and divide by 2 because of spaces between cells
        var moveOrder: [Character: BoardPoint] = [:]

        // Start from index 1 to skip the header line
        for (lineIndex, line) in board.enumerated() where lineIndex > 0 {
            // Get y-coordinate from the beginning of the line, and subtract 1 to start from 0
            let y = (Int(line.prefix(2).trimmingCharacters(in: .whitespaces)) ?? 1) - 1

            // Start parsing after the space that follows the y-coordinate
            for (charIndex, char) in line.dropFirst(3).enumerated() where char == "X" || char == "O" || char.isNumber {
                let xCoord = charIndex / 2
                if char == "X" {
                    blackStones.append(BoardPoint(x: xCoord, y: y))
                } else if char == "O" {
                    whiteStones.append(BoardPoint(x: xCoord, y: y))
                } else {
                    if char.isNumber {
                        moveOrder[char] = BoardPoint(x: xCoord, y: y)
                    }
                }
            }
        }

        return (blackStones, whiteStones, width, height, moveOrder)
    }

    func maybeCollectAnalysis(message: String) {
        if message.starts(with: /info/) {
            let splitData = message.split(separator: "info")
            analysis.data = splitData.map {
                extractMoveData(dataLine: String($0))
            }

            if let lastData = splitData.last {
                analysis.ownership = extractOwnership(message: String(lastData))
            }

            analysis.nextColorForAnalysis = player.nextColorFromShowBoard
        }
    }

    func extractMoveData(dataLine: String) -> [String: String] {
        // Define patterns for extracting relevant information
        let patterns: [String: Regex] = [
            "move": /move (\w+\d+)/,
            "visits": /visits (\d+)/,
            "winrate": /winrate ([\d.eE]+)/,
            "scoreLead": /scoreLead ([-\d.eE]+)/,
            "utilityLcb": /utilityLcb ([-\d.eE]+)/
        ]

        var moveData: [String: String] = [:]
        for (key, pattern) in patterns {
            if let match = dataLine.firstMatch(of: pattern) {
                moveData[key] = String(match.1)
            }
        }

        return moveData
    }

    func extractOwnershipMean(message: String) -> [Float] {
        let pattern = /ownership ([-\d\s.eE]+)/
        if let match = message.firstMatch(of: pattern) {
            let mean = match.1.split(separator: " ").compactMap { Float($0)
            }
            // Return mean if it is valid
            if mean.count == Int(board.width * board.height) {
                return mean
            }
        }

        return []
    }

    func extractOwnershipStdev(message: String) -> [Float] {
        let pattern = /ownershipStdev ([-\d\s.eE]+)/
        if let match = message.firstMatch(of: pattern) {
            let stdev = match.1.split(separator: " ").compactMap { Float($0)
            }
            // Check stdev if it is valid
            if stdev.count == Int(board.width * board.height) {
                return stdev
            }
        }

        return []
    }

    func extractOwnership(message: String) -> [BoardPoint: Ownership] {
        let mean = extractOwnershipMean(message: message)
        let stdev = extractOwnershipStdev(message: message)
        if !mean.isEmpty && !stdev.isEmpty {
            var dictionary: [BoardPoint: Ownership] = [:]
            var i = 0
            for y in stride(from:Int(board.height - 1), through: 0, by: -1) {
                for x in 0..<Int(board.width) {
                    let point = BoardPoint(x: x, y: y)
                    dictionary[point] = Ownership(mean: mean[i], stdev: stdev[i])
                    i = i + 1
                }
            }
            return dictionary
        }

        return [:]
    }
}

struct ContentView_Previews: PreviewProvider {
    static var previews: some View {
        ContentView()
    }
}
