#!/usr/bin/env python3

"""Analysis Engine showboard의 수순·판·정적 응답 계약을 검증한다."""

import argparse
import json
from pathlib import Path
import subprocess
import sys


OVERRIDE = (
    "logFile=,logDir=,logTimeStamp=false,logAllRequests=false,"
    "logAllResponses=false,numAnalysisThreads=1,numSearchThreadsPerAnalysisThread=1,"
    "nnCacheSizePowerOfTwo=16,nnMutexPoolSizePowerOfTwo=12,nnMaxBatchSize=8,"
    "nnRandomize=false,forDeterministicTesting=true"
)


def request(request_id, width=5, height=5, stones=None, moves=None, **options):
    """검증용 직사각형 또는 정사각형 showboard 요청을 만든다."""
    value = {
        "id": request_id, "action": "showboard", "boardXSize": width,
        "boardYSize": height, "rules": "chinese", "komi": 7.5,
        "initialStones": stones or [], "moves": moves or [],
    }
    value.update(options)
    return value


def run(args, requests):
    """한 Analysis Engine 실행에서 요청과 JSON 응답을 모은다."""
    command = [args.katago, "analysis", "-config", args.config, "-model", args.model,
               "-override-config", OVERRIDE]
    process = subprocess.run(
        command, input="".join(json.dumps(value) + "\n" for value in requests),
        text=True, capture_output=True, timeout=120, check=False,
    )
    assert process.returncode == 0, process.stderr[-3000:]
    responses = {}
    for line in process.stdout.splitlines():
        value = json.loads(line)
        if "warning" not in value:
            responses.setdefault(value["id"], []).append(value)
    return responses, process


def assert_board(response, width, height, points, next_player, captures=(0, 0)):
    """행 우선 색 배열, 메타데이터, GTP 본문의 핵심 필드를 검사한다."""
    assert set(response) == {"id", "action", "turnNumber", "isDuringSearch",
                             "nextPlayer", "board", "boardText"}, response
    assert response["action"] == "showboard" and response["isDuringSearch"] is False
    assert response["nextPlayer"] == next_player
    board = response["board"]
    assert set(board) == {"xSize", "ySize", "stones", "koLoc",
                          "numBlackCaptures", "numWhiteCaptures"}, board
    assert (board["xSize"], board["ySize"]) == (width, height)
    expected = ["."] * (width * height)
    for color, point in points:
        x = ord(point[0]) - ord("A")
        y = height - int(point[1:])
        expected[y * width + x] = color
    assert board["stones"] == expected, (board["stones"], expected)
    assert board["numBlackCaptures"] == captures[0]
    assert board["numWhiteCaptures"] == captures[1]
    assert isinstance(board["koLoc"], str)
    player_name = "Black" if next_player == "B" else "White"
    assert f"Next player: {player_name}" in response["boardText"]
    assert response["boardText"].startswith("MoveNum: ")


def gtp_board_text(args, plays=()):
    """같은 모델·규칙·착점의 GTP showboard 본문을 읽는다."""
    config = Path(__file__).resolve().parents[1] / "configs" / "gtp_example.cfg"
    command = [args.katago, "gtp", "-config", str(config), "-model", args.model,
               "-override-config", OVERRIDE + ",logAllGTPCommunication=false"]
    commands = ["boardsize 5", "komi 7.5", "kata-set-rules chinese"]
    commands += [f"play {color} {point}" for color, point in plays]
    commands += ["showboard", "quit"]
    process = subprocess.run(
        command, input="\n".join(commands) + "\n",
        text=True, capture_output=True, timeout=120, check=False,
    )
    assert process.returncode == 0, process.stderr[-3000:]
    messages = [block.strip() for block in process.stdout.split("\n\n") if block.strip()]
    board_messages = [message[2:] for message in messages if message.startswith("= MoveNum: ")]
    assert len(board_messages) == 1, process.stdout
    return board_messages[0]


def test_showboard(args):
    """빈판·초기 돌·패스·포획·연속 색·다중 턴·원자적 오류를 검사한다."""
    capture = [["W", "B2"], ["B", "A2"], ["B", "B1"], ["B", "C2"]]
    requests = [
        request("empty"),
        request("two-moves", moves=[["B", "A1"], ["W", "E5"]]),
        request("rectangle", 4, 3, [["B", "A3"], ["W", "D1"]],
                [["W", "B2"], ["W", "pass"]], analyzeTurns=[0, 1, 2]),
        request("capture", stones=capture, moves=[["B", "B3"]], initialPlayer="B"),
        request("ko", stones=[["W", point] for point in ("B2", "A3", "C3", "B4")]
                + [["B", point] for point in ("A2", "B1", "C2")],
                moves=[["B", "B3"], ["W", "pass"]], initialPlayer="B",
                analyzeTurns=[1, 2]),
        request("illegal", moves=[["B", "A1"], ["W", "B1"], ["B", "A1"]],
                analyzeTurns=[0, 1, 2]),
    ]
    responses, process = run(args, requests)
    assert len(responses["empty"]) == 1
    assert_board(responses["empty"][0], 5, 5, [], "B")
    assert responses["empty"][0]["boardText"] == gtp_board_text(args)
    two_moves = responses["two-moves"][0]
    assert_board(two_moves, 5, 5, [["B", "A1"], ["W", "E5"]], "B")
    assert two_moves["boardText"] == gtp_board_text(args, (("B", "A1"), ("W", "E5")))
    rectangle = sorted(responses["rectangle"], key=lambda value: value["turnNumber"])
    assert [value["turnNumber"] for value in rectangle] == [0, 1, 2]
    assert_board(rectangle[0], 4, 3, [["B", "A3"], ["W", "D1"]], "W")
    assert_board(rectangle[1], 4, 3, [["B", "A3"], ["W", "D1"], ["W", "B2"]], "B")
    assert_board(rectangle[2], 4, 3, [["B", "A3"], ["W", "D1"], ["W", "B2"]], "B")
    assert_board(responses["capture"][0], 5, 5,
                 [["B", "A2"], ["B", "B1"], ["B", "C2"], ["B", "B3"]],
                 "W", (0, 1))
    ko = sorted(responses["ko"], key=lambda value: value["turnNumber"])
    ko_points = [["W", point] for point in ("A3", "C3", "B4")]
    ko_points += [["B", point] for point in ("A2", "B1", "C2", "B3")]
    assert_board(ko[0], 5, 5, ko_points, "W", (0, 1))
    assert_board(ko[1], 5, 5, ko_points, "B", (0, 1))
    assert ko[0]["board"]["koLoc"] == "B2", ko[0]
    assert ko[1]["board"]["koLoc"] == "null", ko[1]
    assert len(responses["illegal"]) == 1, responses["illegal"]
    assert responses["illegal"][0].get("field") == "moves"
    assert "Illegal move 2" in responses["illegal"][0].get("error", "")
    assert any("NN rows:" in line and line.split("NN rows:")[-1].strip() == "0"
               for line in process.stderr.splitlines()), process.stderr[-3000:]


def main():
    """CLI 인자를 읽고 E2E 검사 결과를 종료 코드로 반환한다."""
    parser = argparse.ArgumentParser()
    parser.add_argument("katago")
    parser.add_argument("config")
    parser.add_argument("model")
    args = parser.parse_args()
    try:
        test_showboard(args)
    except (AssertionError, subprocess.TimeoutExpired) as error:
        print(f"analysis showboard E2E: failed: {error}", file=sys.stderr)
        return 1
    print("analysis showboard E2E: passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
