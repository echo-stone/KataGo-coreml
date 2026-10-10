#!/usr/bin/env python3

"""실제 Analysis Engine의 돌 생사 문자열 JSON 계약을 검증한다."""

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


def run_requests(args, requests, extra_config=""):
    """요청 묶음을 한 엔진에 보내고 ID별 응답 목록과 실행 정보를 반환한다."""
    command = [
        args.katago, "analysis", "-config", args.config, "-model", args.model,
        "-override-config", OVERRIDE + extra_config,
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
    """목적: 상태 계약 검증. 매개변수: 응답·필드·턴·프로세스. 반환값: 상태 배열."""
    values = response.get(field)
    if not isinstance(values, list) or len(values) != AREA:
        fail(f"{response.get('id')}: {field} 배열 길이 위반", process)
    occupied = occupied_indexes(turn)
    for index, value in enumerate(values):
        if type(value) is not str or value not in ("A", "E", "D"):
            fail(f"{response.get('id')}: {field}[{index}] 상태 코드 위반: {value!r}", process)
        if (value == "E") != (index not in occupied):
            fail(f"{response.get('id')}: {field}[{index}] 빈칸 코드 위반", process)
    if "seki" in response:
        fail("제거된 seki 응답이 있음", process)
    return values


def analysis_without_status(response):
    """상태 옵션과 무관한 기존 분석 JSON을 비교 가능하게 정규화한다."""
    normalized = copy.deepcopy(response)
    for field in ("id", "alive", "seki"):
        normalized.pop(field, None)
    return normalized


def test_analysis(args):
    """목적: 일반 분석 회귀 검증. 매개변수: CLI 설정. 반환값: 없음."""
    requests = [
        make_request("off"),
        make_request("alive-only", includeAlive=True),
        make_request("ownership-off", includeOwnership=True),
        make_request("ownership-on", includeOwnership=True, includeAlive=True),
        make_request("multi", includeAlive=True, analyzeTurns=[0, 1, 2]),
        make_request("during-search", includeAlive=True,
                     reportDuringSearchEvery=0.005, firstReportDuringSearchAfter=0.005,
                     overrideSettings={"maxVisits": 300}),
        make_request("perspective-w", includeAlive=True,
                     overrideSettings={"maxVisits": 12, "reportAnalysisWinratesAs": "WHITE"}),
    ]
    results, process = run_requests(args, requests)
    final = {request["id"]: final_response(results, request["id"], process)
             for request in requests if request["id"] != "multi"}
    if "alive" in final["off"] or "seki" in final["off"]:
        fail("기본 요청에 상태 배열이 있음", process)
    alive = assert_status(final["alive-only"], "alive", 2, process)
    if analysis_without_status(final["alive-only"]) != analysis_without_status(final["off"]):
        fail("상태 옵션이 기존 분석 결과를 변경함", process)
    owner_off, owner_on = final["ownership-off"], final["ownership-on"]
    if not isinstance(owner_off.get("ownership"), list) or len(owner_off["ownership"]) != AREA:
        fail("기본 ownership 배열 크기 위반", process)
    if analysis_without_status(owner_off) != analysis_without_status(owner_on):
        fail("상태 옵션이 기존 ownership 분석 결과를 변경함", process)
    assert_status(owner_on, "alive", 2, process)
    multi = [message for message in results["multi"] if not message.get("isDuringSearch")]
    if sorted(message.get("turnNumber") for message in multi) != [0, 1, 2]:
        fail("다중 턴 응답 수 또는 턴 번호 위반", process)
    for message in multi:
        assert_status(message, "alive", message["turnNumber"], process)
    if assert_status(final["perspective-w"], "alive", 2, process) != alive:
        fail("흑백 평가 관점에 따라 돌 상태가 변경됨", process)
    if not any(message.get("isDuringSearch") for message in results["during-search"]):
        fail("검색 중 부분 응답이 생성되지 않음", process)
    assert_status(final["during-search"], "alive", 2, process)


def test_actions_and_errors(args):
    """목적: 독립 액션과 제거 오류 검증. 매개변수: CLI 설정. 반환값: 없음."""
    requests = [
        make_request("action-alive", action="alive"),
        make_request("captured", action="alive", initialPlayer="B",
                     initialStones=[["W", "G2"], ["B", "F2"], ["B", "G1"]],
                     moves=[["B", "G3"]], analyzeTurns=[1]),
        make_request("finished-analysis", moves=MOVES + [["W", "pass"], ["B", "pass"]],
                     analyzeTurns=[4], includeAlive=True),
        make_request("finished-alive", action="alive",
                     moves=MOVES + [["W", "pass"], ["B", "pass"]], analyzeTurns=[4]),
        make_request("bad-alive", includeAlive="true"),
        make_request("removed-true", includeSeki=True),
        make_request("removed-false", includeSeki=False),
        make_request("removed-type", includeSeki=1),
        make_request("removed-action", action="seki"),
    ]
    results, process = run_requests(args, requests)
    result = final_response(results, "action-alive", process)
    if set(result) != {"id", "action", "turnNumber", "isDuringSearch", "alive"}:
        fail("독립 액션 응답 형식 위반", process)
    if result["action"] != "alive" or result["turnNumber"] != 2 or result["isDuringSearch"] is not False:
        fail("독립 액션 메타데이터 위반", process)
    assert_status(result, "alive", 2, process)
    captured = final_response(results, "captured", process)["alive"]
    if len(captured) != AREA or captured[1 * WIDTH + 6] != "E":
        fail("포획된 돌의 위치가 E로 직렬화되지 않음", process)
    finished = final_response(results, "finished-analysis", process)
    finished_alive = final_response(results, "finished-alive", process)
    values = assert_status(finished, "alive", 4, process)
    if values != assert_status(finished_alive, "alive", 4, process):
        fail("종료 국면 일반 분석과 독립 alive 불일치", process)
    if values[1 * WIDTH + 6] != "D" or values[0] != "A":
        fail("종료 국면의 죽은 백돌 D 또는 두 눈 흑돌 A 판정 위반", process)
    for request_id, field in [("bad-alive", "includeAlive")] + [
        (name, "includeSeki") for name in ("removed-true", "removed-false", "removed-type")
    ]:
        messages = results[request_id]
        if len(messages) != 1 or not isinstance(messages[0].get("error"), str) or messages[0].get("field") != field:
            fail(f"{request_id}: 명시적 오류 누락", process)
    invalid = results["removed-action"] + results["__unattributed__"]
    if len(invalid) != 1 or not isinstance(invalid[0].get("error"), str):
        fail("제거된 seki 액션 오류 누락", process)
    # 경고 비활성 설정에서도 제거 필드를 허용하지 않는다.
    quiet, quiet_process = run_requests(
        args, [make_request("quiet-removed", includeSeki=False)], ",warnUnusedFields=false"
    )
    if len(quiet["quiet-removed"]) != 1 or quiet["quiet-removed"][0].get("field") != "includeSeki":
        fail("경고 비활성 설정이 제거 오류를 우회함", quiet_process)


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
