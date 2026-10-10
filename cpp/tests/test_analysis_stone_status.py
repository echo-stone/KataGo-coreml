#!/usr/bin/env python3

"""실제 Analysis Engine의 돌 생사·빅 JSON 계약을 검증한다."""

import argparse
import copy
import json
import subprocess
import sys


WIDTH = 7
HEIGHT = 3
AREA = WIDTH * HEIGHT
INITIAL_STONES = [
    ["B", point]
    for point in (
        "A1", "A2", "A3", "B1", "B3", "C1", "C3", "D1", "D2", "D3"
    )
] + [["W", "G2"]]
MOVES = [["W", "G3"], ["B", "E1"]]
OVERRIDE = (
    "logFile=,logDir=,logTimeStamp=false,logAllRequests=false,"
    "logAllResponses=false,maxVisits=40,maxPlayouts=10000,"
    "numAnalysisThreads=1,numSearchThreadsPerAnalysisThread=1,"
    "nnCacheSizePowerOfTwo=16,nnMutexPoolSizePowerOfTwo=12,nnMaxBatchSize=8,"
    "nnRandomize=false,rootSymmetryPruning=false,"
    "nnRandSeed=stoneStatusTest,searchRandSeed=stoneStatusTest,"
    "forDeterministicTesting=true,cudaUseFP16=false,trtUseFP16=false,"
    "openclUseFP16=false,cudaUseNHWC=false"
)


def make_request(request_id, **options):
    """식별자와 선택 옵션을 받아 동일한 직사각형 판 요청을 반환한다."""
    request = {
        "id": request_id,
        "boardXSize": WIDTH,
        "boardYSize": HEIGHT,
        "rules": "chinese",
        "komi": 7.5,
        "initialStones": INITIAL_STONES,
        "moves": MOVES,
        "analyzeTurns": [2],
        "overrideSettings": {"maxVisits": 12},
    }
    request.update(options)
    return request


def fail(message, process):
    """진단 메시지와 엔진 stderr 끝부분을 포함한 실패를 보고한다."""
    tail = "\n".join(process.stderr.splitlines()[-15:])
    raise AssertionError(f"{message}\nanalysis stderr tail:\n{tail}")


def run_requests(args, requests):
    """요청 묶음을 한 엔진에 보내고 ID별 응답 목록과 실행 정보를 반환한다."""
    command = [
        args.katago, "analysis", "-config", args.config, "-model", args.model,
        "-override-config", OVERRIDE,
    ]
    payload = "".join(json.dumps(request, separators=(",", ":")) + "\n" for request in requests)
    process = subprocess.run(
        command, input=payload, text=True, capture_output=True, timeout=180, check=False
    )
    if process.returncode != 0:
        fail(f"analysis 종료 코드 {process.returncode}", process)
    results = {request["id"]: [] for request in requests}
    results["__unattributed__"] = []
    for line_number, raw_line in enumerate(process.stdout.splitlines(), start=1):
        if not raw_line.strip():
            continue
        try:
            value = json.loads(raw_line)
        except json.JSONDecodeError as error:
            fail(f"stdout {line_number} JSON 오류: {error}", process)
        request_id = value.get("id")
        if request_id is None and "error" in value:
            results["__unattributed__"].append(value)
            continue
        if request_id not in results:
            fail(f"stdout {line_number} 알 수 없는 id: {request_id}", process)
        if "warning" in value:
            continue
        results[request_id].append(value)
    return results, process


def final_response(results, request_id, process):
    """한 요청의 최종 결과만 꺼내고 부분 응답의 상태 필드 생략을 검사한다."""
    messages = results[request_id]
    for message in messages:
        if message.get("isDuringSearch"):
            if "alive" in message or "seki" in message:
                fail(f"{request_id}: 검색 중 응답에 상태 배열이 있음", process)
    finals = [message for message in messages if not message.get("isDuringSearch")]
    if len(finals) != 1:
        fail(f"{request_id}: 최종 응답 수 {len(finals)}", process)
    final = finals[0]
    if "error" in final or final.get("noResults"):
        fail(f"{request_id}: 최종 분석 실패: {final}", process)
    return final


def occupied_indexes(turn):
    """초기 돌과 지정 턴까지의 수순을 행 우선 배열의 점유 인덱스로 변환한다."""
    stones = INITIAL_STONES + MOVES[:turn]
    indexes = set()
    for _, point in stones:
        x = ord(point[0]) - ord("A")
        y = HEIGHT - int(point[1])
        indexes.add(y * WIDTH + x)
    return indexes


def assert_status(response, field, turn, process):
    """상태 배열의 크기·불리언·빈칸 규칙을 검증한다."""
    values = response.get(field)
    if not isinstance(values, list) or len(values) != AREA:
        fail(f"{response.get('id')}: {field} 배열 길이 위반", process)
    occupied = occupied_indexes(turn)
    for index, value in enumerate(values):
        if type(value) is not bool:
            fail(f"{response.get('id')}: {field}[{index}]가 불리언이 아님", process)
        if index not in occupied and value:
            fail(f"{response.get('id')}: {field}[{index}] 빈칸이 true", process)
    return values


def assert_subset(seki, alive, label, process):
    """같은 판의 빅 참 값이 모두 생존 참 값인지 검증한다."""
    if any(is_seki and not is_alive for is_seki, is_alive in zip(seki, alive)):
        fail(f"{label}: seki가 alive 부분집합이 아님", process)


def analysis_without_status(response):
    """상태 옵션과 무관한 기존 분석 JSON을 비교 가능하게 정규화한다."""
    normalized = copy.deepcopy(response)
    for field in ("id", "alive", "seki"):
        normalized.pop(field, None)
    return normalized


def test_analysis(args):
    """일반 분석의 옵션·여러 턴·기존 ownership 보존을 검사한다."""
    requests = [
        make_request("off"),
        make_request("alive-only", includeAlive=True),
        make_request("seki-only", includeSeki=True),
        make_request("both", includeAlive=True, includeSeki=True),
        make_request("ownership-off", includeOwnership=True),
        make_request("ownership-on", includeOwnership=True, includeAlive=True, includeSeki=True),
        make_request("multi", includeAlive=True, includeSeki=True, analyzeTurns=[0, 1, 2]),
        make_request(
            "during-search", includeAlive=True, includeSeki=True,
            reportDuringSearchEvery=0.005, firstReportDuringSearchAfter=0.005,
            overrideSettings={"maxVisits": 300},
        ),
        make_request(
            "perspective-w", includeAlive=True, includeSeki=True,
            overrideSettings={"maxVisits": 12, "reportAnalysisWinratesAs": "WHITE"},
        ),
    ]
    results, process = run_requests(args, requests)
    final = {
        request["id"]: final_response(results, request["id"], process)
        for request in requests if request["id"] != "multi"
    }
    if "alive" in final["off"] or "seki" in final["off"]:
        fail("기본 요청에 상태 배열이 있음", process)
    if "seki" in final["alive-only"] or "alive" in final["seki-only"]:
        fail("개별 요청에 미요청 배열이 있음", process)
    alive = assert_status(final["alive-only"], "alive", 2, process)
    seki = assert_status(final["seki-only"], "seki", 2, process)
    both_alive = assert_status(final["both"], "alive", 2, process)
    both_seki = assert_status(final["both"], "seki", 2, process)
    if alive != both_alive or seki != both_seki:
        fail("개별 요청과 동시 요청의 배열 불일치", process)
    assert_subset(both_seki, both_alive, "both", process)
    for request_id in ("off", "alive-only", "seki-only", "both"):
        if analysis_without_status(final[request_id]) != analysis_without_status(final["off"]):
            fail(f"{request_id}: 기존 분석 결과가 상태 옵션으로 변경됨", process)
    owner_off = final["ownership-off"]
    owner_on = final["ownership-on"]
    if not isinstance(owner_off.get("ownership"), list) or len(owner_off["ownership"]) != AREA:
        fail("기본 ownership 배열 크기 위반", process)
    if owner_off["ownership"] != owner_on.get("ownership"):
        fail("상태 옵션이 기존 ownership 결과를 변경함", process)
    if analysis_without_status(owner_off) != analysis_without_status(owner_on):
        fail("상태 옵션이 기존 ownership 분석 결과를 변경함", process)
    assert_subset(
        assert_status(owner_on, "seki", 2, process),
        assert_status(owner_on, "alive", 2, process),
        "ownership-on", process,
    )
    multi_finals = [message for message in results["multi"] if not message.get("isDuringSearch")]
    if sorted(message.get("turnNumber") for message in multi_finals) != [0, 1, 2]:
        fail("다중 턴 응답 수 또는 턴 번호 위반", process)
    for message in multi_finals:
        turn = message["turnNumber"]
        assert_subset(
            assert_status(message, "seki", turn, process),
            assert_status(message, "alive", turn, process),
            f"multi/{turn}", process,
        )
    perspective = final["perspective-w"]
    white_seki = assert_status(perspective, "seki", 2, process)
    white_alive = assert_status(perspective, "alive", 2, process)
    assert_subset(white_seki, white_alive, "perspective-w", process)
    if white_alive != both_alive or white_seki != both_seki:
        fail("흑백 평가 관점에 따라 돌 상태가 뒤집히거나 변경됨", process)
    if not any(message.get("isDuringSearch") for message in results["during-search"]):
        fail("during-search: 검색 중 부분 응답이 생성되지 않음", process)
    assert_subset(
        assert_status(final["during-search"], "seki", 2, process),
        assert_status(final["during-search"], "alive", 2, process),
        "during-search final", process,
    )


def test_actions_and_errors(args):
    """독립 액션의 응답 경계와 잘못된 입력의 오류 봉투를 검증한다."""
    requests = [
        make_request("action-alive", action="alive"),
        make_request("action-seki", action="seki"),
        make_request(
            "finished-analysis", moves=MOVES + [["W", "pass"], ["B", "pass"]],
            analyzeTurns=[4], includeAlive=True, includeSeki=True,
        ),
        make_request(
            "finished-alive", action="alive",
            moves=MOVES + [["W", "pass"], ["B", "pass"]], analyzeTurns=[4],
        ),
        make_request(
            "finished-seki", action="seki",
            moves=MOVES + [["W", "pass"], ["B", "pass"]], analyzeTurns=[4],
        ),
        make_request("bad-alive", includeAlive="true"),
        make_request("bad-seki", includeSeki=1),
        make_request("bad-action", action="stone_status_unknown"),
    ]
    results, process = run_requests(args, requests)
    statuses = {}
    for request_id, field in (("action-alive", "alive"), ("action-seki", "seki")):
        result = final_response(results, request_id, process)
        expected = {"id", "action", "turnNumber", "isDuringSearch", field}
        if set(result) != expected or result["action"] != field or result["turnNumber"] != 2:
            fail(f"{request_id}: 독립 액션 응답 형식 위반: {sorted(result)}", process)
        if result["isDuringSearch"] is not False:
            fail(f"{request_id}: 독립 액션이 검색 중 응답으로 표시됨", process)
        statuses[field] = assert_status(result, field, 2, process)
    assert_subset(statuses["seki"], statuses["alive"], "독립 액션", process)
    finished = final_response(results, "finished-analysis", process)
    finished_alive = final_response(results, "finished-alive", process)
    finished_seki = final_response(results, "finished-seki", process)
    if finished["turnNumber"] != 4:
        fail("finished-analysis: 종료 턴 번호 위반", process)
    if assert_status(finished, "alive", 4, process) != assert_status(finished_alive, "alive", 4, process):
        fail("종료 국면 일반 분석과 독립 alive 판정 불일치", process)
    if assert_status(finished, "seki", 4, process) != assert_status(finished_seki, "seki", 4, process):
        fail("종료 국면 일반 분석과 독립 seki 판정 불일치", process)
    for request_id, field in (("bad-alive", "includeAlive"), ("bad-seki", "includeSeki")):
        messages = results[request_id]
        if len(messages) != 1 or not isinstance(messages[0].get("error"), str):
            fail(f"{request_id}: 오류 응답 누락", process)
        if messages[0].get("field") != field:
            fail(f"{request_id}: 오류 필드 불일치: {messages[0]}", process)
    invalid_action = results["bad-action"] + results["__unattributed__"]
    if len(invalid_action) != 1 or not isinstance(invalid_action[0].get("error"), str):
        fail("bad-action: 오류 응답 누락", process)


def main():
    """CLI 인자를 해석하고 엔진 E2E 결과를 종료 코드로 반환한다."""
    parser = argparse.ArgumentParser()
    parser.add_argument("katago")
    parser.add_argument("config")
    parser.add_argument("model")
    args = parser.parse_args()
    try:
        test_analysis(args)
        test_actions_and_errors(args)
    except (AssertionError, subprocess.TimeoutExpired) as error:
        print(f"analysis stone status E2E: failed: {error}", file=sys.stderr)
        return 1
    print("analysis stone status E2E: passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
