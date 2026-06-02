# Analysis Engine final_score Action Design

## Summary

Add an analysis-engine special action named `final_score` that computes only the GTP-style final score estimate for requested positions. The action should reuse the same board, rules, komi, move-history, override, and turn-selection parsing as normal analysis requests, but it should not emit `moveInfos`, ownership, policy, or other analysis fields.

The goal is to expose the same behavior as the GTP `final_score` command without making every normal analysis response slower or changing existing `scoreLead` semantics.

## Current Behavior

Normal analysis requests enqueue one `AnalyzeRequest` per selected turn. Each worker sets an `AsyncBot` position, runs normal search, and writes JSON from `Search::getAnalysisJson`.

Special actions such as `query_version`, `clear_cache`, `terminate`, `terminate_all`, and `query_models` are currently handled near the front of the input loop. These are cheap or administrative actions. `final_score` is different because it may run multiple short searches while probing komi, so it must not block the input loop.

GTP `final_score` uses `GTPEngine::computeAnticipatedWinnerAndScore`:

```text
if the game is already finished under a supported final-scoring state:
  use hist.winner and hist.finalWhiteMinusBlackScore
else:
  visits = max(50, numThreads * 10)
  lead = PlayUtils::computeLead(...)
  round lead to integer or half-integer depending on rules
  winner = sign(lead)
```

In plain language, unfinished positions are scored by finding the komi that would make the game roughly even, then comparing that fair komi against the current komi.

## API

### Request

Use a special action:

```json
{
  "id": "foo",
  "action": "final_score",
  "boardXSize": 19,
  "boardYSize": 19,
  "initialStones": [],
  "moves": [["B","D4"],["W","Q16"]],
  "rules": "chinese",
  "komi": 7.5
}
```

The action accepts the normal position-building fields used by analysis requests:

| Field group | Behavior |
| --- | --- |
| `boardXSize`, `boardYSize`, `initialStones`, `moves`, `initialPlayer`, `rules`, `komi`, `whiteHandicapBonus` | Same validation and board-history construction as normal analysis. |
| `analyzeTurns`, `priorities` | Same meaning as normal analysis. If absent, score the final position after all moves. |
| `overrideSettings`, `maxVisits` | Same validation path as normal analysis. `overrideSettings` may affect parameters such as `numThreads`; `maxVisits` is accepted for parser compatibility but does not override the GTP-equivalent final score probe budget. |
| Analysis-only output options | Ignored for this action. If these fields are parsed by shared request code, they must not affect the final-score response. |

### Response

One response is emitted per selected turn:

```json
{
  "id": "foo",
  "action": "final_score",
  "turnNumber": 2,
  "isDuringSearch": false,
  "winner": "W",
  "finalScore": "W+3.5",
  "finalWhiteMinusBlackScore": 3.5,
  "scoreSource": "estimated"
}
```

`winner` is `"B"`, `"W"`, or `"0"`. `finalScore` matches GTP formatting: `"B+N.N"`, `"W+N.N"`, or `"0"`. `finalWhiteMinusBlackScore` is positive when white is ahead and negative when black is ahead. `scoreSource` is `"finished"` when the board history already has a final score, otherwise `"estimated"`.

## Architecture

Add a request kind to the existing queued request structure:

```text
AnalyzeRequest
  kind = ANALYSIS | FINAL_SCORE
  shared fields: id, turnNumber, priority, board, hist, nextPla, params
  analysis-only fields remain used only by ANALYSIS
```

This keeps a single priority queue, termination map, worker pool, logger, and `AsyncBot` lifecycle. It also preserves the existing behavior that each selected turn produces one final response.

Worker flow:

```text
analysisLoop
  pop request
  set bot position, params, avoid/include data
  if request.kind == ANALYSIS:
    run existing analysis flow
  else if request.kind == FINAL_SCORE:
    compute final score response
    push JSON response
  clear bot search
  remove request from openRequests
```

The input loop should parse `action: "final_score"` after reading the `id`, but it should continue through the same position parser instead of returning early like cheap special actions. Unknown action validation must include `final_score`.

## Final Score Computation

Implement a helper in `analysis.cpp` or a small local helper near the analysis loop:

```text
computeAnalysisFinalScore(bot, board, hist, nextPla, params)
```

The helper should mirror GTP behavior:

1. Save and restore the bot's old params and position.
2. Apply the same bias-removal settings used by GTP final score:
   - `playoutDoublingAdvantage = 0`
   - `conservativePass = true`
   - all human SL chosen/exploration probabilities = `0`
   - `antiMirror = false`
   - `avoidRepeatedPatternUtility = 0`
3. If `hist.isGameFinished` and the rules are in the same finished-score cases as GTP, use `hist.winner` and `hist.finalWhiteMinusBlackScore`.
4. Otherwise call `PlayUtils::computeLead(bot->getSearchStopAndWait(), NULL, board, hist, nextPla, max(50, params.numThreads * 10), OtherGameProperties())`. This preserves the GTP `final_score` visit budget rather than using the normal analysis `maxVisits`.
5. Round to nearest integer for integer-result rules, otherwise nearest half-integer.
6. Format `"B+N.N"`, `"W+N.N"`, or `"0"`.

## Errors And Termination

Validation errors should use the existing `reportErrorForId` shape. A malformed `final_score` request should report the specific field error, just like a malformed analysis request.

Because `final_score` requests are queued, `terminate` and `terminate_all` should be able to cancel them before the worker starts. Once the worker is inside `PlayUtils::computeLead`, cancellation is best-effort only unless deeper cancellation wiring is added. This matches the current spirit of termination, which is best-effort and asynchronous.

If a queued final-score request is terminated before any computation starts, the existing `noResults` response shape is acceptable:

```json
{"id":"foo","turnNumber":2,"isDuringSearch":false,"noResults":true}
```

## Documentation

Update `docs/Analysis_Engine.md` special actions with:

- `final_score` request fields.
- Response fields and examples.
- A note that the action may run several short searches and is therefore queued with normal analysis work.
- A note that `finalWhiteMinusBlackScore` is white-positive.

## Testing

Add focused tests for:

| Test | Expected result |
| --- | --- |
| Parsing `action: "final_score"` with a legal empty-board request | Enqueues and returns a `final_score` response instead of `moveInfos`. |
| Multiple `analyzeTurns` | Emits one final-score response per selected turn. |
| Finished-game path | Uses recorded final score and returns `scoreSource: "finished"`. |
| Unfinished path with test backend | Returns a formatted score and `scoreSource: "estimated"`. |
| Invalid action or malformed field | Existing JSON error style is preserved. |
| Terminated queued final-score request | Returns `noResults` if stopped before computation starts. |

Run verification:

```text
cpp/xcode/coreml-build.sh
targeted command/analysis tests if available
manual analysis-engine smoke test with the built binary and test model/config when available
```

If a full smoke test cannot run because no model/config is available, record that limitation and rely on compile plus targeted unit/command tests.

## Non-Goals

- Do not change normal `scoreLead`, `scoreMean`, or `scoreSelfplay` semantics.
- Do not add `finalScore` to every normal analysis response.
- Do not alter GTP `final_score`.
- Do not add a persistent cache for final-score estimates in this change.
