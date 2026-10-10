#ifndef PROGRAM_PLAY_UTILS_H_
#define PROGRAM_PLAY_UTILS_H_

#include "../core/config_parser.h"
#include "../program/play.h"
#include "../search/asyncbot.h"

//This is a grab-bag of various useful higher-level functions that select moves or evaluate the board in various ways.

namespace PlayUtils {
  //Use the given bot to play free handicap stones, modifying the board and hist in the process and setting the bot's position to it.
  //Does NOT switch the initial player of the board history to white
  void playExtraBlack(
    Search* bot,
    int numExtraBlack,
    Board& board,
    BoardHistory& hist,
    double temperature,
    Rand& gameRand
  );

  //Set board to empty and place fixed handicap stones, raising an exception if invalid
  void placeFixedHandicap(Board& board, int n);

  ExtraBlackAndKomi chooseExtraBlackAndKomi(
    float base, float stdev, double allowIntegerProb,
    double handicapProb, int numExtraBlackFixed,
    double bigStdevProb, float bigStdev,
    double biggerStdevProb, float biggerStdev,
    double sqrtBoardArea, Rand& rand
  );
  void setKomiWithoutNoise(const ExtraBlackAndKomi& extraBlackAndKomi, BoardHistory& hist); //Also ignores allowInteger
  void setKomiWithNoise(const ExtraBlackAndKomi& extraBlackAndKomi, BoardHistory& hist, Rand& rand);

  ReportedSearchValues getWhiteScoreValues(
    Search* bot,
    const Board& board,
    const BoardHistory& hist,
    Player pla,
    int64_t numVisits,
    const OtherGameProperties& otherGameProps
  );

  Loc chooseRandomLegalMove(const Board& board, const BoardHistory& hist, Player pla, Rand& gameRand, Loc banMove);
  int chooseRandomLegalMoves(const Board& board, const BoardHistory& hist, Player pla, Rand& gameRand, Loc* buf, int len);

  Loc chooseRandomPolicyMove(
    const NNOutput* nnOutput,
    const Board& board,
    const BoardHistory& hist,
    Player pla,
    Rand& gameRand,
    double temperature,
    bool allowPass,
    Loc banMove
  );

  Loc getGameInitializationMove(
    Search* botB, Search* botW, const Board& board, const BoardHistory& hist, Player pla, NNResultBuf& buf,
    Rand& gameRand, double temperature
  );
  void initializeGameUsingPolicy(
    Search* botB, Search* botW, Board& board, BoardHistory& hist, Player& pla,
    Rand& gameRand, bool doEndGameIfAllPassAlive,
    double proportionOfBoardArea, double policyInitGammaShape, double temperature
  );

  float roundAndClipKomi(double unrounded, const Board& board);

  void adjustKomiToEven(
    Search* botB,
    Search* botW, //can be NULL if only one bot
    const Board& board,
    BoardHistory& hist,
    Player pla,
    int64_t numVisits,
    const OtherGameProperties& otherGameProps,
    Rand& rand
  );

  //Lead from WHITE's perspective
  float computeLead(
    Search* botB,
    Search* botW, //can be NULL if only one bot
    const Board& board,
    BoardHistory& hist,
    Player pla,
    int64_t numVisits,
    const OtherGameProperties& otherGameProps
  );

  // 목적: 취소 신호를 확인하면서 백 관점의 예상 점수 차이를 계산한다.
  // 매개변수: botB와 botW는 검색기이고, board와 hist 및 pla는 위치, numVisits는 탐색당 방문 수,
  // otherGameProps는 대국 속성, shouldStopEarly는 취소 여부 콜백, result는 계산된 점수 차이를 받을 변수다.
  // 반환값: 계산을 마치면 true, 취소 신호로 중단하면 false를 반환한다.
  bool computeLeadCancellable(
    Search* botB,
    Search* botW, //can be NULL if only one bot
    const Board& board,
    BoardHistory& hist,
    Player pla,
    int64_t numVisits,
    const OtherGameProperties& otherGameProps,
    std::function<bool()>& shouldStopEarly,
    float& result
  );

  double getSearchFactor(
    double searchFactorWhenWinningThreshold,
    double searchFactorWhenWinning,
    const SearchParams& params,
    const std::vector<double>& recentWinLossValues,
    Player pla
  );

  double getHackedLCBForWinrate(const Search* search, const AnalysisData& data, Player pla);

  std::vector<double> computeOwnership(
    Search* bot,
    const Board& board,
    const BoardHistory& hist,
    Player pla,
    int64_t numVisits,
    std::function<bool()>* shouldStopEarly = nullptr
  );

  //Determine all living and dead stones, if the game were terminated right now and
  //the rules were interpreted naively and directly.
  //Returns a vector indexed by board Loc (length Board::MAX_ARR_SIZE).
  std::vector<bool> computeAnticipatedStatusesSimple(
    const Board& board,
    const BoardHistory& hist
  );

  //Determine all living and dead stones, trying to be clever and use the ownership prediction
  //of the neural net.
  //Returns a vector indexed by board Loc (length Board::MAX_ARR_SIZE).
  std::vector<bool> computeAnticipatedStatusesWithOwnership(
    Search* bot,
    const Board& board,
    const BoardHistory& hist,
    Player pla,
    int64_t numVisits,
    std::vector<double>& ownershipsBuf,
    std::function<bool()>* shouldStopEarly = nullptr
  );

  // ownerships의 NN 격자 예측값으로 현재 board의 돌무리별 예상 생존을 계산한다.
  // nnXLen/nnYLen은 예측 격자 크기이며, 반환 배열은 Board::MAX_ARR_SIZE 길이의 Loc 인덱스다.
  std::vector<bool> computeAnticipatedStatusesFromOwnership(
    const Board& board,
    const std::vector<double>& ownerships,
    int nnXLen,
    int nnYLen
  );

  // 계가 기준 빅 후보 중 살아 있다고 예상된 현재 돌만 표시한다.
  // board/hist는 국면과 자충 규칙, isAlive는 Loc별 예상 생존이며 반환 배열은 Loc별 빅 후보다.
  std::vector<bool> computeAnticipatedSekiStatuses(
    const Board& board,
    const BoardHistory& hist,
    const std::vector<bool>& isAlive
  );


  struct BenchmarkResults {
    int numThreads = 0;
    int totalPositionsSearched = 0;
    int totalPositions = 0;
    int64_t totalVisits = 0;
    double totalSeconds = 0;
    int64_t numNNEvals = 0;
    int64_t numNNBatches = 0;
    double avgBatchSize = 0;

    std::string toStringNotDone() const;
    std::string toString() const;
    std::string toStringWithElo(const BenchmarkResults* baseline, double secondsPerGameMove) const;

    double computeEloEffect(double secondsPerGameMove) const;

    static void printEloComparison(const std::vector<BenchmarkResults>& results, double secondsPerGameMove);
  };

  //Run benchmark on sgf positions. ALSO prints to stdout the ongoing result as it benchmarks.
  BenchmarkResults benchmarkSearchOnPositionsAndPrint(
    const SearchParams& params,
    const CompactSgf& sgf,
    int numPositionsToUse,
    NNEvaluator* nnEval,
    const BenchmarkResults* baseline,
    double secondsPerGameMove,
    bool printElo
  );

  void printGenmoveLog(
    std::ostream& out,
    const Search* search,
    const NNEvaluator* nnEval,
    Loc moveLoc,
    double timeTaken,
    Player perspective,
    bool logSearchInfoForChosenMove
  );

  Rules genRandomRules(Rand& rand);

  Loc maybeCleanupBeforePass(
    enabled_t cleanupBeforePass,
    enabled_t friendlyPass,
    const Player pla,
    Loc moveLoc,
    const AsyncBot* bot
  );

  Loc maybeFriendlyPass(
    enabled_t cleanupBeforePass,
    enabled_t friendlyPass,
    const Player pla,
    Loc moveLoc,
    Search* bot,
    int64_t numVisits
  );

  std::shared_ptr<NNOutput> getFullSymmetryNNOutput(
    const Board& board, const BoardHistory& hist, Player pla, bool includeOwnerMap, const SGFMetadata* sgfMeta, NNEvaluator* nnEval
  );

}


#endif //PROGRAM_PLAY_UTILS_H_
