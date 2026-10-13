#!/usr/bin/env python3
"""일반 분석 보드·생사·계가 옵션과 기존 독립 계가 계약을 검증한다."""

import argparse
import itertools
import json
import math
import subprocess
import sys


OVERRIDE = (
    "logFile=,logDir=,logTimeStamp=false,logAllRequests=false,"
    "logAllResponses=false,numAnalysisThreads=1,numSearchThreadsPerAnalysisThread=1,"
    "nnCacheSizePowerOfTwo=16,nnMutexPoolSizePowerOfTwo=12,nnMaxBatchSize=8,"
    "nnRandomize=false,forDeterministicTesting=true"
)


def request(request_id, **options):
    """같은 5×5 수순을 사용해 선택 옵션별 분석 요청을 만든다."""
    value = {
        "id": request_id, "boardXSize": 5, "boardYSize": 5,
        "rules": "chinese", "komi": 7.5,
        "initialStones": [], "moves": [["B", "A1"]],
        "analyzeTurns": [0, 1], "overrideSettings": {"maxVisits": 8},
    }
    value.update(options)
    return value


def run(args, requests):
    """한 엔진 실행에 요청 묶음을 전달하고 ID별 응답을 모은다."""
    command = [args.katago, "analysis", "-config", args.config, "-model", args.model,
               "-override-config", OVERRIDE]
    process = subprocess.run(
        command, input="".join(json.dumps(value) + "\n" for value in requests),
        text=True, capture_output=True, timeout=240, check=False,
    )
    assert process.returncode == 0, process.stderr[-3000:]
    responses = {}
    for line in process.stdout.splitlines():
        value = json.loads(line)
        if "warning" not in value:
            responses.setdefault(value["id"], []).append(value)
    return responses


def check_board(board, turn, alive):
    """공통 메타데이터와 행 우선 점 배열의 타입·크기를 검사한다."""
    assert set(board) == {"xSize", "ySize", "stones", "numBlackCaptures",
                          "numWhiteCaptures"}, board
    assert (board["xSize"], board["ySize"]) == (5, 5)
    assert (board["numBlackCaptures"], board["numWhiteCaptures"]) == (0, 0)
    stones = board["stones"]
    assert len(stones) == 25 and all(isinstance(point, str) for point in stones)
    assert set(stones) <= ({"B", "W", "b", "w", ".", "!"} if alive else
                           {"B", "W", ".", "!"})
    assert stones.count("!") <= 1
    if turn == 0:
        assert set(stones) == {"."}
    else:
        assert stones[20] in ({"B", "b"} if alive else {"B"})


def check_score(score):
    """승자, 형식화된 결과, 양수 차이가 일치하는지 확인한다."""
    assert set(score) == {"winner", "result", "margin"}, score
    assert score["winner"] in {"B", "W", "0"}
    assert isinstance(score["margin"], (int, float))
    assert math.isfinite(score["margin"]) and score["margin"] >= 0
    expected = "0" if score["winner"] == "0" else f'{score["winner"]}+{score["margin"]:.1f}'
    assert score["result"] == expected, score


def test_options(args):
    """8개 독립 조합, 실제 턴, legacy 유지, final_score 생사 필드를 검사한다."""
    requests = []
    for board, alive, score in itertools.product((False, True), repeat=3):
        name = f"options-{int(board)}{int(alive)}{int(score)}"
        requests.append(request(name, includeBoard=board, includeAliveBoard=alive,
                                includeFinalScore=score))
    requests.append(request("legacy", includeAlive=True, analyzeTurns=[1]))
    requests.append(request("old-score", action="final_score", analyzeTurns=[1]))
    requests.append(request("score-v2", action="final_score", includeAliveBoard=True,
                            analyzeTurns=[1]))
    requests.append(request("bad-board", includeBoard="true", analyzeTurns=[1]))
    requests.append(request("bad-alive-board", includeAliveBoard="true", analyzeTurns=[1]))
    requests.append(request("bad-final-score", includeFinalScore="true", analyzeTurns=[1]))
    ko_stones = [["W", point] for point in ("B2", "A3", "C3", "B4")]
    ko_stones += [["B", point] for point in ("A2", "B1", "C2")]
    ko_options = {"initialStones": ko_stones, "initialPlayer": "B",
                  "moves": [["B", "B3"]], "analyzeTurns": [1]}
    requests.append(request("ko-analysis", includeBoard=True, includeAliveBoard=True,
                            includeFinalScore=True, **ko_options))
    requests.append(request("ko-score", action="final_score", includeAliveBoard=True,
                            **ko_options))
    ended = {"moves": [["B", "pass"], ["W", "pass"]], "analyzeTurns": [2]}
    requests.append(request("finished-analysis", includeBoard=True,
                            includeAliveBoard=True, includeFinalScore=True, **ended))
    requests.append(request("finished-score", action="final_score",
                            includeAliveBoard=True, **ended))
    responses = run(args, requests)

    for board, alive, score in itertools.product((False, True), repeat=3):
        name = f"options-{int(board)}{int(alive)}{int(score)}"
        complete = [value for value in responses[name] if not value.get("isDuringSearch")]
        assert len(complete) == 2, (name, responses[name])
        for turn, value in enumerate(sorted(complete, key=lambda item: item["turnNumber"])):
            assert value["turnNumber"] == turn
            assert ("board" in value) == board, (name, value)
            assert ("aliveBoard" in value) == alive, (name, value)
            assert ("finalScoreResult" in value) == score, (name, value)
            assert "alive" not in value and "finalScore" not in value
            if board:
                check_board(value["board"], turn, False)
            if alive:
                check_board(value["aliveBoard"], turn, True)
            if score:
                check_score(value["finalScoreResult"])
        for value in responses[name]:
            if value.get("isDuringSearch"):
                assert not {"board", "aliveBoard", "finalScoreResult"} & value.keys()

    legacy = responses["legacy"][0]
    assert "alive" in legacy and "aliveBoard" not in legacy
    assert isinstance(legacy["alive"], list) and len(legacy["alive"]) == 25
    old_score = responses["old-score"][0]
    new_score = responses["score-v2"][0]
    assert "nextPlayer" not in old_score and "aliveBoard" not in old_score
    assert new_score["nextPlayer"] == "W"
    check_board(new_score["aliveBoard"], 1, True)
    assert responses["bad-board"][0]["field"] == "includeBoard"
    assert responses["bad-alive-board"][0]["field"] == "includeAliveBoard"
    assert responses["bad-final-score"][0]["field"] == "includeFinalScore"
    ko_index = 3 * 5 + 1
    ko_analysis = responses["ko-analysis"][0]
    ko_score = responses["ko-score"][0]
    for value in (ko_analysis, ko_score):
        assert value["aliveBoard"]["stones"][ko_index] == "!", value
        assert value["aliveBoard"]["stones"].count("!") == 1, value
    assert ko_analysis["board"]["stones"][ko_index] == "!", ko_analysis
    finished_analysis = responses["finished-analysis"][0]
    finished_score = responses["finished-score"][0]
    assert finished_analysis["finalScoreResult"]["result"] == finished_score["finalScore"]
    assert finished_analysis["aliveBoard"] == finished_score["aliveBoard"]


def main():
    """CLI 입력을 해석하고 검증 결과를 종료 코드로 반환한다."""
    parser = argparse.ArgumentParser()
    parser.add_argument("katago")
    parser.add_argument("config")
    parser.add_argument("model")
    args = parser.parse_args()
    try:
        test_options(args)
    except (AssertionError, subprocess.TimeoutExpired) as error:
        print(f"analysis board score v2 E2E: failed: {error}", file=sys.stderr)
        return 1
    print("analysis board score v2 E2E: passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
