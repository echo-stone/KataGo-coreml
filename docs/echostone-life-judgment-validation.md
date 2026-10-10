# 에코스톤 문제 DB 사활 판정 검증

## 범위와 결론

2026-10-10 에코스톤 운영 DB의 `public.problems`에서 생사 정답형 7개를 전부
조회하고, 일반 사활의 살기·잡기·빅에서 각각 1개를 추가 선정했다.
**직접적인 생사 정답이 있는 7개는 일반 분석과 독립 `alive` 액션 모두 DB 정답과
일치했다. 생사 정답형에서는 `seki`가 생존으로 판정한 흑돌 전부에서 참이므로, 실제 빅만 구분하는
값으로 사용할 수 없다.**

추가 빅 문제에서는 내부 흑 5점·백 10점과 외곽 흑 10점을 구분했다.
최종 실행은 **10문제, 51개 분석 응답, 출력 계약 위반 0건**이다.

DB 연결은 읽기 전용 트랜잭션 설정으로 조회했으며 데이터는 변경하지 않았다.
[원본 입력 스냅샷](fixtures/echostone-life-judgment-samples.json)에 문제 ID,
분류, 지문, SGF, 객관식 선지와 정답을 보관했다. 계정 정보와 접속 비밀값은
포함하지 않았다.

## 실행 조건

- 엔진: `dfa99153`에서 빌드한 KataGo v1.18.2 통합 바이너리.
- 백엔드: Apple M4 Max의 Metal MPSGraph GPU.
- 모델: `kata1-zhizi-b40c768nbt-fdx6d-s11272M-d5935M`.
- 판: 원본 그대로 19×19. SGF의 `aa`는 A19, 배열은 좌상단부터 행 우선.
- 규칙: 원본에 RU·KM이 없어 일본식·덤 0집을 실험 조건으로 사용.
- 일반 분석: 128 visits, `includeAlive`, `includeSeki`, `includeOwnership`.
- 독립 액션: GTP 생사 판정용 전용 검색을 사용하는 `alive`, `seki`, 각 100 visits.
- 분석·검색 스레드 각 1개, 고정 난수 설정, `preventCleanupPhase=false`.

## 생사 정답형 7개

7개 모두 `life and death / 모양구분 / 판단` 분류의 `지문객관식`이다.
정답은 `quiz_choices[quiz_answer_index]`에서 읽었다. 원본에 선번과 대상돌
표식이 없어 흑·백 양쪽 선번으로 검사하고, 흑 배치석 전체와 체인별 결과를
기록했다. 모든 문제에서 체인 사이에 생존 판정 차이는 없었다.

표의 숫자는 **참으로 나온 흑돌 수**다. 흑·백 어느 쪽 선번에서도 같은 결과였다.

| 문제 ID | DB 정답 | 흑돌 수 | 일반 alive / seki | 독립 alive / seki |
| --- | --- | ---: | ---: | ---: |
| 1016084 | 죽어있다 | 5 | 0 / 0 | 0 / 0 |
| 1016085 | 살아있다 | 9 | 9 / 9 | 9 / 9 |
| 1016086 | 살아있다 | 6 | 6 / 6 | 6 / 6 |
| 1016087 | 죽어있다 | 8 | 0 / 0 | 0 / 0 |
| 1016127 | 살아있다 | 8 | 8 / 8 | 8 / 8 |
| 1016128 | 죽어있다 | 8 | 0 / 0 | 0 / 0 |
| 1016129 | 살아있다 | 10 | 10 / 10 | 10 / 10 |

7문제 × 2선번 × 3요청의 **42응답**을 확인했다. 일반 분석 14응답은 모두
실제 방문 수 128이었다. 배열 길이 361, 불리언 타입, 빈칸 거짓, 동일 체인 내
균일성, 일반 분석의 `seki ⊆ alive`에서 위반이 없었다. 즉 같은 일반 응답에서
빅으로 표시한 돌은 반드시 살아 있다고도 표시됐다. 독립 두 액션은 서로 다른
검색이지만 이번 실행에서는 14국면 모두 같은 포함 관계를 보였다.

이 7개에는 미생이나 빅을 정답으로 하는 문제가 없다. 따라서 이번 일치 결과는
생존·사망 답을 확인한 것이며, 미생·빅 분류 정확도를 검증한 결과는 아니다.

## 일반 사활 추가 3개

직접 생사 정답형은 7개뿐이므로 10개 검증의 나머지는 일반 사활 문제로
구성했다. 살기·잡기·빅별로 조건을 만족하는 문제를
`md5(id::text || ':20261010')` 순서로 정렬해 첫 문제를 골랐다. 원본 SGF의
정답 표식 `C[RIGHT]`로 이어지는 수순을 두고 다음 선번에서 검사한다.

정답 수순의 끝은 대국 종료가 아니다. 특히 잡기 문제의 올바른 수를 뒀어도
백돌이 즉시 잡히지 않을 수 있으므로, 이 세 문제는 객관식 생사 정답 일치율에
합산하지 않는다.

원본의 오답 분기가 더 긴 두 문제는 `loadsgf`가 그 분기를 선택할 수 있다.
KataGo의 SGF 로더는 가장 긴 분기를 선택하므로, 정답 분기만 분리한 SGF를
재생해 Analysis Engine에 보낸 수순과 동일한지 확인한다.

| 문제 ID | 유형 | 정답 수 | 확인 대상 | 일반 alive / seki | 독립 alive / seki |
| --- | --- | --- | --- | ---: | ---: |
| 1042061 | 살기 | 흑 Q1 | 착수 포함 흑 11점 | 11 / 11 | 11 / 11 |
| 1015713 | 잡기 | 흑 A3 | 백 9점 | 0 / 0 | 0 / 0 |
| 38017 | 빅 | 흑 R19 | 내부 흑 5점·백 10점 | 15 / 15 | 15 / 15 |
| 38017 | 위와 같은 국면 | 위와 같음 | 외곽 흑 10점 | 10 / 0 | 10 / 0 |

세 문제 모두 정답 수 직후 포획은 없었고, 다음 선번은 백이었다. 잡기 문제의
백 9점은 판 위에 남아 있는 상태에서 모두 `alive=false`로 판정됐다. 살기
문제는 흑 11점이 한 체인으로 살아 있다고 판정됐으며, 내부의 백 5점은
`alive=false`였다.

빅 문제의 핵심 흑 체인은 `P19, R19, P18, Q18, R18`이다. 백은 9점 체인과
T19의 한 점 체인으로 나뉜다. 외곽을 고정해 국소 수읽기를 검토하면, 공동
활로를 먼저 메우는 쪽이 상대에게 잡히는 구조다. 이 15점을 빅으로 표시하고
외곽 흑 10점을 제외한 결과는 해당 국소 형태와 부합한다. 한편 살기 문제의
흑 11점도 전부 `seki=true`였으므로, 빅 문제 한 건의 구분 성공을 전체
`seki` 판별 정확도로 일반화할 수 없다.

추가 9응답도 배열·빈칸·체인·포함 관계 검사에서 위반이 없었다. 총 51응답의
최종 분석 실행은 종료 코드 0이었으며, 일반 분석 17응답 모두 실제 방문 수
128이었다. GTP로도 정답 분리 SGF 세 개를 읽어 돌 배치, 수 1, 포획 0,
다음 선번 백이 Analysis Engine 입력과 일치함을 확인했다.

## 재현 입력과 실행 기록

- [생사 정답형 42요청](fixtures/echostone-life-judgment-requests.jsonl)
- [일반 사활 추가 9요청](fixtures/echostone-life-supplemental-requests.jsonl)

DB에 다시 접속하지 않고 저장된 요청 파일로 재현할 수 있다. 새 체크아웃은
`cpp/xcode/coreml-build.sh --jobs 8`로 먼저 빌드하고, 실제 모델 경로를 지정해
각 요청 파일을 **별도 엔진 프로세스**에 입력한다.

```sh
cpp/build-coreml/katago analysis \
  -config cpp/configs/analysis_example.cfg \
  -model /Users/tk/.katago/model.bin.gz \
  -override-config 'logFile=,logDir=,logTimeStamp=false,logAllRequests=false,logAllResponses=false,logToStderr=true,maxVisits=128,numAnalysisThreads=1,numSearchThreadsPerAnalysisThread=1,numNNServerThreadsPerModel=1,metalDeviceToUseThread0=0,nnCacheSizePowerOfTwo=16,nnMutexPoolSizePowerOfTwo=12,nnMaxBatchSize=8,nnRandomize=false,rootSymmetryPruning=false,nnRandSeed=echostoneLife20261010,searchRandSeed=echostoneLife20261010,forDeterministicTesting=true,preventCleanupPhase=false' \
  < docs/fixtures/echostone-life-judgment-requests.jsonl
```

추가 검사는 입력 파일만 `echostone-life-supplemental-requests.jsonl`로 바꿔 실행한다.

원시 응답, 전체 체인·돌별 결과, 모델 초기화 로그와 GTP 재생 로그는
`echostone-life-validation-20261010` 검증 워크트리의
`cpp/build-coreml/echostone-life-validation/`에 보관했다. 일반 42요청은
55.211초, 추가 9요청과 GTP 재생은 20.387초가 걸렸다. 빌드 산출물과 실행
로그는 커밋하지 않는다. 엔진 코드와 DB 데이터의 변경은 없다.
