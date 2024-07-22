//
//  ContentView.swift
//  KataGo iOS
//
//  Created by Chin-Chang Yang on 2023/7/2.
//

import SwiftUI
import SwiftData
import KataGoInterface

struct ContentView: View {
    @StateObject var stones = Stones()
    @StateObject var messagesObject = MessagesObject()
    @StateObject var board = ObservableBoard()
    @StateObject var player = PlayerObject()
    @StateObject var analysis = Analysis()
    @StateObject var config = Config()
    @State private var isShowingBoard = false
    @State private var boardText: [String] = []
    @Query var gameRecords: [GameRecord]
    @Environment(\.modelContext) private var modelContext
    @StateObject var gobanState = GobanState()
    @StateObject var winrate = Winrate()
    @State private var navigationContext = NavigationContext()

    init() {
        // Start a thread to run KataGo GTP
        Thread {
            KataGoHelper.runGtp()
        }.start()
    }

    var body: some View {
        NavigationSplitView {
            List(selection: $navigationContext.selectedGameRecord) {
                ForEach(gameRecords) { gameRecord in
                    NavigationLink("Goban", value: gameRecord)
                }
                .onDelete(perform: { indexSet in
                    for index in indexSet {
                        let gameRecordToDelete = gameRecords[index]
                        if navigationContext.selectedGameRecord?.persistentModelID == gameRecordToDelete.persistentModelID {
                            navigationContext.selectedGameRecord = nil
                        }

                        modelContext.delete(gameRecordToDelete)
                    }
                })
            }
            .navigationTitle("Menu")
        } detail: {
            GobanView(gameRecord: navigationContext.selectedGameRecord)
        }
        .environmentObject(stones)
        .environmentObject(messagesObject)
        .environmentObject(board)
        .environmentObject(player)
        .environmentObject(analysis)
        .environmentObject(config)
        .environmentObject(gobanState)
        .environmentObject(winrate)
        .environment(navigationContext)
        .onAppear() {
            // Get messages from KataGo and append to the list of messages
            createMessageTask()
        }
        .onChange(of: gobanState.waitingForAnalysis) { waitedForAnalysis, waitingForAnalysis in
            if (waitedForAnalysis && !waitingForAnalysis) {
                if gobanState.analysisStatus == .pause {
                    KataGoHelper.sendCommand("stop")
                } else {
                    KataGoHelper.sendCommand(config.getKataAnalyzeCommand())
                }
            }
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
            if !gameRecords.isEmpty { navigationContext.selectedGameRecord = gameRecords[0] }
            maybeLoadSgf()
            KataGoHelper.sendCommand("showboard")
            KataGoHelper.sendCommand("printsgf")
            gobanState.requestAnalysis(config: config)

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

                // Collect SGF information
                maybeCollectSgf(message: line)

                // Remove when there are too many messages
                while messagesObject.messages.count > config.maxMessageLines {
                    messagesObject.messages.removeFirst()
                }
            }
        }
    }

    func maybeLoadSgf() {
        if let gameRecord = navigationContext.selectedGameRecord {
            let sgf = gameRecord.sgf

            let supportDirectory =
            try? FileManager.default.url(for: .documentDirectory,
                                         in: .userDomainMask,
                                         appropriateFor: nil,
                                         create: true)

            if let supportDirectory {
                let file = supportDirectory.appendingPathComponent("temp.sgf")
                do {
                    try sgf.write(to: file, atomically: false, encoding: .utf8)
                    let path = file.path()
                    KataGoHelper.sendCommand("loadsgf \(path)")
                } catch {
                    // Do nothing
                }
            }
        }
    }

    func maybeCollectBoard(message: String) {
        if isShowingBoard {
            if message.prefix("Next player".count) == "Next player" {
                isShowingBoard = false
                var newBoardWidth: CGFloat
                var newBoardHeight: CGFloat
                (stones.blackPoints, stones.whitePoints, newBoardWidth, newBoardHeight, stones.moveOrder) = parseBoardPoints(board: boardText)
                if (newBoardWidth != board.width) || (newBoardHeight != board.height) {
                    analysis.clear()
                }
                board.width = newBoardWidth
                board.height = newBoardHeight
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

    func getBlackWinrate() -> Float {
        guard !analysis.info.isEmpty else { return 0.5 }
        let points = analysis.info.keys.sorted()

        let visits = points.map { point in
            analysis.info[point]?.visits ?? 0
        }

        let sumVisits = visits.reduce(1, +)

        let weightedWinrates = points.map() { point in
            let winrate = analysis.info[point]?.winrate ?? 0.5
            let visit = analysis.info[point]?.visits ?? 0
            return winrate * Float(visit) / Float(sumVisits)
        }

        let winrate = weightedWinrates.reduce(0, +)
        let blackWinrate = (analysis.nextColorForAnalysis == .black) ? winrate : (1 - winrate)

        return blackWinrate
    }

    func maybeCollectAnalysis(message: String) {
        if message.starts(with: /info/) {
            let splitData = message.split(separator: "info")

            withAnimation {
                let analysisInfo = splitData.map {
                    extractAnalysisInfo(dataLine: String($0))
                }

                analysis.info = analysisInfo.reduce([:]) {
                    $0.merging($1 ?? [:]) { (current, _) in
                        current
                    }
                }

                if let lastData = splitData.last {
                    analysis.ownership = extractOwnership(message: String(lastData))
                }

                analysis.nextColorForAnalysis = player.nextColorFromShowBoard
                winrate.black = getBlackWinrate()
            }

            gobanState.waitingForAnalysis = false
        }
    }

    func moveToPoint(move: String) -> BoardPoint? {
        let pattern = /([^\d\W]+)(\d+)/
        if let match = move.firstMatch(of: pattern),
           let coordinate = Coordinate(xLabel: String(match.1),
                                       yLabel: String(match.2)) {
            // Subtract 1 from y to make it 0-indexed
            return BoardPoint(x: coordinate.x, y: coordinate.y - 1)
        } else {
            return nil
        }
    }

    func matchMovePattern(dataLine: String) -> BoardPoint? {
        let pattern = /move (\w+\d+)/
        if let match = dataLine.firstMatch(of: pattern) {
            let move = String(match.1)
            if let point = moveToPoint(move: move) {
                return point
            }
        }

        return nil
    }

    func matchVisitsPattern(dataLine: String) -> Int? {
        let pattern = /visits (\d+)/
        if let match = dataLine.firstMatch(of: pattern) {
            let visits = Int(match.1)
            return visits
        }

        return nil
    }

    func matchWinratePattern(dataLine: String) -> Float? {
        let pattern = /winrate ([-\d.eE]+)/
        if let match = dataLine.firstMatch(of: pattern) {
            let winrate = Float(match.1)
            return winrate
        }

        return nil
    }

    func matchScoreLeadPattern(dataLine: String) -> Float? {
        let pattern = /scoreLead ([-\d.eE]+)/
        if let match = dataLine.firstMatch(of: pattern) {
            let scoreLead = Float(match.1)
            return scoreLead
        }

        return nil
    }

    func matchUtilityLcbPattern(dataLine: String) -> Float? {
        let pattern = /utilityLcb ([-\d.eE]+)/
        if let match = dataLine.firstMatch(of: pattern) {
            let scoreLead = Float(match.1)
            return scoreLead
        }

        return nil
    }

    func extractAnalysisInfo(dataLine: String) -> [BoardPoint: AnalysisInfo]? {
        let point = matchMovePattern(dataLine: dataLine)
        let visits = matchVisitsPattern(dataLine: dataLine)
        let winrate = matchWinratePattern(dataLine: dataLine)
        let scoreLead = matchScoreLeadPattern(dataLine: dataLine)
        let utilityLcb = matchUtilityLcbPattern(dataLine: dataLine)

        if let point, let visits, let winrate, let scoreLead, let utilityLcb {
            let analysisInfo = AnalysisInfo(visits: visits, winrate: winrate, scoreLead: scoreLead, utilityLcb: utilityLcb)

            return [point: analysisInfo]
        }

        return nil
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

    func maybeCollectSgf(message: String) {
        let sgfPrefix = "= (;FF[4]GM[1]"
        if message.hasPrefix(sgfPrefix) {
            if let startOfSgf = message.firstIndex(of: "(") {
                let sgfString = String(message[startOfSgf...])
                let lastMoveIndex = SgfHelper(sgf: sgfString).getLastMoveIndex() ?? -1
                let currentIndex = lastMoveIndex + 1
                if gameRecords.isEmpty {
                    modelContext.insert(GameRecord(sgf: sgfString, currentIndex: currentIndex))
                } else if let gameRecord = navigationContext.selectedGameRecord {
                    gameRecord.sgf = sgfString
                    gameRecord.currentIndex = currentIndex
                }
            }
        }
    }
}

#Preview {
    let config = ModelConfiguration(isStoredInMemoryOnly: true)
    let container = try! ModelContainer(for: GameRecord.self, configurations: config)

    return ContentView()
        .modelContainer(container)
}
