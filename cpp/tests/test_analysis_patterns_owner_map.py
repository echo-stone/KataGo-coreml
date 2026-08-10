#!/usr/bin/env python3

import argparse
import copy
import json
import re
import subprocess
import sys


BOARD_AREA = 7 * 3
SUPPORTED_RULES = json.dumps(
    {
        "ko": "SITUATIONAL",
        "score": "AREA",
        "tax": "NONE",
        "suicide": False,
        "hasButton": False,
        "whiteHandicapBonus": "N-1",
        "friendlyPassOk": True,
    },
    separators=(",", ":"),
)
UNSUPPORTED_RULES = SUPPORTED_RULES.replace("SITUATIONAL", "SPIGHT")
INITIAL_STONES = [
    ["B", point]
    for point in (
        "A1", "A2", "A3", "B1", "B3", "C1", "C3", "D1", "D2",
        "D3", "E1", "E3", "F1", "F3", "G1", "G2", "G3",
    )
]
FINISHING_STATS_RE = re.compile(
    r"^(?:: )?GPU -?\d+ finishing, processed (\d+) rows (\d+) batches, owner map rows (\d+)$"
)


def fail(message, process=None):
    """계약 위반을 진단 가능한 짧은 메시지로 종료한다."""
    if process is not None and process.stderr:
        message += "\nanalysis stderr tail:\n" + "\n".join(process.stderr.splitlines()[-20:])
    raise AssertionError(message)


def make_request(request_id, include_patterns, **options):
    """동일 국면과 검색 설정을 쓰는 분석 요청을 만든다."""
    request = {
        "id": request_id,
        "initialStones": INITIAL_STONES,
        "moves": [],
        "rules": SUPPORTED_RULES,
        "komi": 7.5,
        "boardXSize": 7,
        "boardYSize": 3,
        "analyzeTurns": [0],
        "analysisPVLen": 3,
        "includePVVisits": True,
        "includePatterns": include_patterns,
        "overrideSettings": {"maxVisits": 12},
    }
    request.update(options)
    return request


def normalized_response(response):
    """패턴 옵션이 의도적으로 바꾸는 id와 patterns만 비교에서 제외한다."""
    normalized = copy.deepcopy(response)
    normalized.pop("id", None)
    normalized.pop("patterns", None)
    return normalized


def visit_signature(response):
    """루트와 각 후보 수의 방문 수·PV 방문 수를 결정적 튜플로 만든다."""
    root_visits = response["rootInfo"]["visits"]
    move_visits = tuple(
        (
            move["move"],
            move["visits"],
            move.get("edgeVisits"),
            tuple(move.get("pvVisits", [])),
            tuple(move.get("pvEdgeVisits", [])),
        )
        for move in response["moveInfos"]
    )
    return root_visits, move_visits


def assert_search_shape(response):
    """고정 방문 수와 moveInfos·PV 방문 수의 production 응답 계약을 검사한다."""
    if response.get("rootInfo", {}).get("visits") != 12:
        fail(f"{response.get('id')}: root visits가 12가 아님")
    move_infos = response.get("moveInfos")
    if not isinstance(move_infos, list) or not move_infos:
        fail(f"{response.get('id')}: moveInfos가 비어 있음")
    for move in move_infos:
        pv_visits = move.get("pvVisits")
        if not isinstance(pv_visits, list) or not pv_visits:
            fail(f"{response.get('id')}/{move.get('move')}: pvVisits가 비어 있음")


def parse_backend_stats(request_id, stderr):
    """backend 종료 로그를 엄격히 파싱해 처리 행과 owner-map 행 합계를 반환한다."""
    finishing_lines = [
        line for line in stderr.splitlines() if " finishing, processed " in line
    ]
    if not finishing_lines:
        fail(f"{request_id}: backend 종료 통계가 없음")
    processed_rows = 0
    owner_map_rows = 0
    for line in finishing_lines:
        match = FINISHING_STATS_RE.fullmatch(line)
        if match is None:
            fail(f"{request_id}: backend 종료 통계 형식 위반: {line}")
        processed_rows += int(match.group(1))
        owner_map_rows += int(match.group(3))
    return processed_rows, owner_map_rows


def compare_patterns_pair(responses, off_id, on_id):
    """패턴 토글 전후의 분석 값과 모든 방문 수가 같은지 비교한다."""
    off = responses[off_id]
    on = responses[on_id]
    if "patterns" in off:
        fail(f"{off_id}: includePatterns=false 응답에 patterns가 존재함")
    if not isinstance(on.get("patterns"), list):
        fail(f"{on_id}: includePatterns=true 응답의 patterns가 배열이 아님")
    normalized_off = normalized_response(off)
    normalized_on = normalized_response(on)
    if normalized_off != normalized_on:
        different_fields = sorted(
            key for key in set(normalized_off) | set(normalized_on)
            if normalized_off.get(key) != normalized_on.get(key)
        )
        fail(f"{off_id}/{on_id}: patterns와 id 외 분석 JSON이 달라짐: {different_fields}")
    if visit_signature(off) != visit_signature(on):
        fail(f"{off_id}/{on_id}: 루트 또는 후보 수 visits가 달라짐")


def assert_root_array(response, field):
    """루트 ownership 계열 배열의 보드 크기 계약을 검사한다."""
    value = response.get(field)
    if not isinstance(value, list) or len(value) != BOARD_AREA:
        fail(f"{response.get('id')}: {field} 배열 길이가 {BOARD_AREA}가 아님")


def assert_move_array(response, field):
    """모든 후보 수 ownership 계열 배열의 보드 크기 계약을 검사한다."""
    move_infos = response.get("moveInfos")
    if not isinstance(move_infos, list) or not move_infos:
        fail(f"{response.get('id')}: moveInfos가 비어 있음")
    for move in move_infos:
        value = move.get(field)
        if not isinstance(value, list) or len(value) != BOARD_AREA:
            fail(f"{response.get('id')}/{move.get('move')}: {field} 배열 길이 위반")


def run_request(args, request, override):
    """한 요청을 독립 analysis 프로세스에서 실행해 응답·warning·backend 통계를 반환한다."""
    command = [
        args.katago,
        "analysis",
        "-config",
        args.config,
        "-model",
        args.model,
        "-override-config",
        override,
    ]
    process = subprocess.run(
        command,
        input=json.dumps(request, separators=(",", ":")) + "\n",
        text=True,
        capture_output=True,
        timeout=180,
        check=False,
    )
    if process.returncode != 0:
        fail(f"{request['id']}: analysis 프로세스 종료 코드 {process.returncode}", process)

    response = None
    warnings = []
    for line_number, raw_line in enumerate(process.stdout.splitlines(), start=1):
        if not raw_line.strip():
            continue
        try:
            value = json.loads(raw_line)
        except json.JSONDecodeError as error:
            fail(f"{request['id']}: stdout {line_number} JSON 파싱 실패: {error}", process)
        if value.get("id") != request["id"]:
            fail(f"{request['id']}: 다른 id의 analysis 메시지 수신", process)
        if "error" in value:
            fail(f"{request['id']}: analysis 오류: {value['error']}", process)
        if "warning" in value:
            warnings.append(value)
        elif "rootInfo" in value:
            if response is not None:
                fail(f"{request['id']}: 분석 응답 중복")
            response = value
        else:
            fail(f"{request['id']}: stdout {line_number} 알 수 없는 analysis 메시지")
    if response is None:
        fail(f"{request['id']}: 분석 응답 없음", process)
    processed_rows, owner_map_rows = parse_backend_stats(request["id"], process.stderr)
    return response, warnings, processed_rows, owner_map_rows


def run_test(args):
    """실제 analysis 프로세스의 패턴·ownership 비용 경계 계약을 검증한다."""
    requests = [
        make_request("patterns-off", False),
        make_request("patterns-on", True),
        make_request("patterns-suppressed", True, rules=UNSUPPORTED_RULES),
        make_request("ownership-patterns-off", False, includeOwnership=True),
        make_request("ownership-patterns-on", True, includeOwnership=True),
        make_request("ownership-stdev-patterns-off", False, includeOwnershipStdev=True),
        make_request("ownership-stdev-patterns-on", True, includeOwnershipStdev=True),
        make_request("moves-ownership-patterns-off", False, includeMovesOwnership=True),
        make_request("moves-ownership-patterns-on", True, includeMovesOwnership=True),
        make_request("moves-ownership-stdev-patterns-off", False, includeMovesOwnershipStdev=True),
        make_request("moves-ownership-stdev-patterns-on", True, includeMovesOwnershipStdev=True),
    ]
    override = (
        "logFile=,logDir=,logTimeStamp=false,logAllRequests=false,logAllResponses=false,"
        "logSearchInfo=false,maxVisits=100,maxPlayouts=10000,numAnalysisThreads=1,"
        "numSearchThreadsPerAnalysisThread=1,nnRandomize=false,rootSymmetryPruning=false,"
        "nnRandSeed=analysisPatternOwnerMapTest,searchRandSeed=analysisPatternOwnerMapTest,"
        "forDeterministicTesting=true,cudaUseFP16=false,trtUseFP16=false,"
        "openclUseFP16=false,cudaUseNHWC=false"
    )
    responses = {}
    warnings = {}
    processed_rows = {}
    owner_map_rows = {}
    for request in requests:
        response, request_warnings, request_rows, request_owner_map_rows = run_request(
            args, request, override
        )
        responses[request["id"]] = response
        processed_rows[request["id"]] = request_rows
        owner_map_rows[request["id"]] = request_owner_map_rows
        if request_warnings:
            warnings[request["id"]] = request_warnings

    expected_ids = {request["id"] for request in requests}
    if set(responses) != expected_ids:
        fail(f"분석 응답 id 불일치: {sorted(set(responses) ^ expected_ids)}")
    for response in responses.values():
        assert_search_shape(response)

    compare_patterns_pair(responses, "patterns-off", "patterns-on")
    if not responses["patterns-on"]["patterns"]:
        fail("patterns-on: 활성 5종 패턴 국면에서 patterns가 비어 있음")
    active_kinds = {"bensonAlive", "eye", "ladder", "nakade", "semeai"}
    emitted_kinds = {pattern.get("kind") for pattern in responses["patterns-on"]["patterns"]}
    if not emitted_kinds <= active_kinds:
        fail(f"patterns-on: 비활성 Kind가 출력됨: {sorted(emitted_kinds - active_kinds)}")

    suppressed = responses["patterns-suppressed"]
    if suppressed.get("patterns") != []:
        fail("patterns-suppressed: 룰 대체 응답이 빈 patterns가 아님")
    if normalized_response(responses["patterns-off"]) != normalized_response(suppressed):
        fail("patterns-suppressed: 지원 룰 기준 응답과 patterns/id 외 JSON이 달라짐")
    if visit_signature(responses["patterns-off"]) != visit_signature(suppressed):
        fail("patterns-suppressed: 지원 룰 기준 루트/후보 visits와 달라짐")

    root_only_ids = ("patterns-off", "patterns-on", "patterns-suppressed")
    root_only_counts = {owner_map_rows[request_id] for request_id in root_only_ids}
    if root_only_counts != {1}:
        fail(f"패턴 토글·억제의 root-only owner map rows가 1로 같지 않음: {owner_map_rows}")
    for request_id in root_only_ids:
        if processed_rows[request_id] <= owner_map_rows[request_id]:
            fail(f"{request_id}: root-only보다 많은 검색 행이 관측되지 않음")

    base = responses["patterns-off"]
    if "ownership" in base or "ownershipStdev" in base:
        fail("patterns-off: 요청하지 않은 루트 ownership 필드가 존재함")
    for move in base["moveInfos"]:
        if "ownership" in move or "ownershipStdev" in move:
            fail("patterns-off: 요청하지 않은 후보 수 ownership 필드가 존재함")

    comparison_pairs = [
        ("ownership-patterns-off", "ownership-patterns-on"),
        ("ownership-stdev-patterns-off", "ownership-stdev-patterns-on"),
        ("moves-ownership-patterns-off", "moves-ownership-patterns-on"),
        ("moves-ownership-stdev-patterns-off", "moves-ownership-stdev-patterns-on"),
    ]
    for off_id, on_id in comparison_pairs:
        compare_patterns_pair(responses, off_id, on_id)
        if owner_map_rows[off_id] != owner_map_rows[on_id]:
            fail(f"{off_id}/{on_id}: owner map rows가 다름")
        if owner_map_rows[off_id] <= 1:
            fail(f"{off_id}/{on_id}: ownership 요청이 root-only보다 owner map rows를 늘리지 않음")

    for request_id in ("ownership-patterns-off", "ownership-patterns-on"):
        assert_root_array(responses[request_id], "ownership")
    for request_id in ("ownership-stdev-patterns-off", "ownership-stdev-patterns-on"):
        assert_root_array(responses[request_id], "ownershipStdev")
    for request_id in ("moves-ownership-patterns-off", "moves-ownership-patterns-on"):
        assert_move_array(responses[request_id], "ownership")
    for request_id in ("moves-ownership-stdev-patterns-off", "moves-ownership-stdev-patterns-on"):
        assert_move_array(responses[request_id], "ownershipStdev")

    unexpected_warning_ids = set(warnings) - {"patterns-suppressed"}
    if unexpected_warning_ids:
        fail(f"예상하지 않은 warning id: {sorted(unexpected_warning_ids)}")
    suppressed_warnings = warnings.get("patterns-suppressed", [])
    by_field = {warning.get("field"): warning.get("warning", "") for warning in suppressed_warnings}
    if set(by_field) != {"rules", "patterns"} or len(suppressed_warnings) != 2:
        fail("patterns-suppressed: rules/patterns warning 두 건 계약 위반")
    if "not supported by neural net" not in by_field["rules"] or "using" not in by_field["rules"]:
        fail("patterns-suppressed: rules 대체 warning 내용 위반")
    pattern_warning = by_field["patterns"]
    for token in ("RULES_SUBSTITUTED", "SPIGHT", "SITUATIONAL", "patternsSuppressed=true"):
        if token not in pattern_warning:
            fail(f"patterns-suppressed: patterns warning에 {token} 없음")

    print("analysis includePatterns owner-map E2E: passed")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("katago")
    parser.add_argument("config")
    parser.add_argument("model")
    args = parser.parse_args()
    try:
        run_test(args)
    except (AssertionError, subprocess.TimeoutExpired) as error:
        print(f"analysis includePatterns owner-map E2E: failed: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
