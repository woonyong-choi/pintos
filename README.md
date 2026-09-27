# PintOS — 스레드 스케줄링과 가상 메모리 학습

KAIST PintOS x86-64 골격에 스레드 스케줄링, 사용자 프로세스, 가상 메모리를 구현한 팀 학습 프로젝트입니다.

- 팀 기간에는 alarm·priority donation과 userprog·vm 전반을 맡았습니다. 팀원은 파일 시스템 콜, 프레임 테이블, 교체 알고리즘, mmap 본체를 담당했습니다.
- 과제 종료 후 MLFQS와 Copy-on-Write fork를 구현하고, 전체 테스트로 드러나지 않던 선점 오류 두 건을 재현 테스트부터 추가해 수정했습니다.
- CI의 x86 QEMU 환경에서 threads 28/28, userprog 96/96, vm 142/142를 확인했습니다. 팀/개인 변경의 커밋 단위 경계는 [담당 범위 문서](docs/contribution.md)에 기록했습니다.

## 데모

QEMU에서 실제로 `make check`를 실행한 화면입니다(threads 테스트 6개, 2배속 재생). `alarm-single`·`alarm-priority`·`priority-change`·`priority-donate-one`·`priority-donate-nest`·`priority-sema`가 모두 통과해 "All 6 tests passed."가 출력됩니다. `pass` 줄이 두 번 나오는 것은 테스트 실행 중 한 번, `make check` 요약에서 한 번 출력되기 때문입니다. Apple Silicon에서 Docker(linux/amd64 에뮬레이션)로 돌렸고, 재현 명령은 아래 "빠르게 실행하기" 4번입니다.

![PintOS 테스트 실행 GIF](https://raw.githubusercontent.com/woonyong-choi/lrn-pintos/main/docs/demo.gif)

## 빠르게 실행하기

x86-64 Linux, GCC, Make, Perl, QEMU가 필요합니다. macOS(arm64)에서는 저장소의 x86-64 Dev Container를 씁니다. 아래 명령은 2026-09-23에 같은 저장소 상태에서 실행해 통과를 확인했습니다.

```bash
git clone https://github.com/woonyong-choi/lrn-pintos.git && cd lrn-pintos
docker build --platform linux/amd64 -t pintos-dev .devcontainer/
docker run --rm --platform linux/amd64 -v "$PWD:/w" -w /w pintos-dev \
  bash -lc 'source pintos/activate && make -C pintos/userprog check'   # All 96 tests passed.
docker build --load -t pintos-demo -f scripts/Dockerfile.demo scripts && bash scripts/demo_check.sh   # All 6 tests passed.
```

threads·vm 전체 스위트는 CI가 실행합니다(아래 "검증" 절). `activate`는 저장소의 `pintos/utils`를 PATH에 추가하고, 각 명령은 커널을 빌드한 뒤 QEMU에서 검사를 실행합니다. 개별 실행 출력은 해당 디렉터리의 `build/tests/`에 남습니다.

## 구조

```
lrn-pintos/
├── pintos/
│   ├── threads/      # 스케줄링 · 동기화 · MLFQS
│   ├── userprog/     # 시스템 콜 · 프로세스 생명주기
│   ├── vm/           # 페이지 · 프레임 · 스왑 · Copy-on-Write
│   ├── filesys/      # 파일 시스템(과제 제공, 미확장)
│   └── tests/        # 스위트별 회귀 테스트
├── docs/          # 설계 노트 · 단계별 구현 기록 · 담당 범위
├── scripts/       # 데모 실행 스크립트(Docker + QEMU)
└── .github/workflows/ci.yml   # threads · userprog · vm CI
```

```mermaid
flowchart LR
  T["타이머 인터럽트"] --> TH["threads<br/>스케줄링 · 동기화"]
  S["시스템 콜"] --> UP["userprog<br/>프로세스 생명주기"]
  P["페이지 폴트"] --> VM["vm<br/>페이지 · 프레임 · 스왑"]
  UP --> VM
```

세 진입점(타이머 인터럽트·시스템 콜·페이지 폴트)이 각각 threads·userprog·vm으로 들어가고, userprog는 프로세스가 페이지를 요구하는 지점에서 vm과 다시 연결됩니다. 왜 이 자료구조를 골랐는지는 아래 "핵심 결정과 트레이드오프"에, 전체 설계 근거는 [설계 노트](docs/design.md)와 [단계별 구현 기록](docs/pintos/)에 있습니다.

## 핵심 결정과 트레이드오프

- **`ready_list` 정렬 불변식과 그것이 두 번 깨진 이유** — 문제: 스케줄러가 항상 최고 우선순위 스레드를 골라야 한다. 선택: `list_insert_ordered()`로 항상 정렬된 단일 연결 리스트를 유지하고 `next_thread_to_run()`은 front만 `O(1)`로 pop한다. 버린 대안: 64개 우선순위 버킷(4.4BSD 원본 방식) — 삽입·선택 모두 `O(1)`이지만, 이 커널은 스레드가 수십 개뿐이라 `O(n)` 차이가 측정 가능한 수준이 아니고 버킷은 초기화·MLFQS 재계산·donation 세 지점에서 일관성을 따로 지켜야 한다. 근거: 실제로 그 지점 중 하나를 놓쳤다 — `thread_create()`가 새 스레드의 실제 `t->priority` 대신 호출자가 넘긴 인자로 선점을 판단해 MLFQS에서만 오동작했고(기존 8개 priority-donate 테스트가 모두 *실행 중인* 스레드에게만 기부해 못 잡음), lock holder가 ready 상태로 대기 중일 때 기부가 값만 바뀌고 큐 위치는 그대로여서 priority inversion이 되돌아왔다. 두 버그 모두 재현 테스트([`priority-donate-ready.c`](pintos/tests/threads/priority-donate-ready.c))를 먼저 쓰고 `thread_set_effective_priority()`로 "값 쓰기 + 큐 위치 이동"의 소유자를 하나로 통일해 고쳤다. → [`threads/thread.c`](pintos/threads/thread.c), [`threads/synch.c`](pintos/threads/synch.c), [설계 노트 §1](docs/design.md)
- **MLFQS 17.14 고정소수점** — 문제: 커널이 FPU를 쓰지 않는데 4.4BSD `recent_cpu` 감쇠 계수(`(2·load_avg)/(2·load_avg+1)`)는 1보다 작아 정수 나눗셈으로는 0 또는 1로 뭉개진다. 선택: `int32`를 정수부 17비트·소수부 14비트로 분할하고 곱셈·나눗셈은 `int64`로 승격한다. 버린 대안: 16.16 — 소수부 해상도는 좋아지지만 `recent_cpu` 상한(±32767)을 보장할 근거가 없어 오버플로 위험만 늘어난다. 갱신 대상도 ready 스레드가 아니라 `all_elem`으로 유지하는 전체 스레드(blocked 포함)여야 했다 — 자고 있는 스레드도 감쇠해야 깨어났을 때 우선순위가 맞기 때문이다. → [`threads/thread.c`](pintos/threads/thread.c), [설계 노트 §2](docs/design.md)
- **accessed bit를 쓰는 clock 프레임 교체** — 문제: 메모리 압박 시 내보낼 프레임을 골라야 하는데 정확한 LRU는 매핑된 페이지의 접근이 fault를 내지 않아 커널이 관찰할 수 없다. 선택: `clock_hand`가 프레임 테이블을 원형으로 돌며 `pml4_is_accessed()`가 켜진 프레임은 비트를 내리고 한 번 봐주는 second-chance 방식(최대 2바퀴). 버린 대안: FIFO — 구현은 절반이지만 hot page와 오래 안 쓴 페이지를 구분 못 해 방금 쓴 페이지를 내보내고 곧바로 다시 읽어들인다. 한계: Copy-on-Write로 공유 중인 프레임(`ref_count > 1`)은 후보에서 제외되는데, 이건 정책이 아니라 `struct frame`이 소유자를 하나만 들고 있어 공유 프레임을 내릴 때 나머지 공유자의 매핑을 찾을 방법이 없는 구조적 제약이다. → [`vm/vm.c`](pintos/vm/vm.c), [설계 노트 §3](docs/design.md)
- **Copy-on-Write fork** — 문제: 기본 `fork`는 부모의 모든 페이지를 자식에게 복사하지만, `fork` 직후 `exec`하는 흔한 패턴에서는 그 복사가 전부 폐기된다. 선택: `fork` 직후 양쪽 페이지 테이블이 같은 프레임을 읽기 전용으로 공유하고(`ref_count++`), 쓰기 page fault가 나면 `ref_count > 1`이면 새 프레임에 복사, `== 1`이면 복사 없이 쓰기 권한만 복원한다. `page->writable`(원래 권한)과 pml4 쓰기 비트(현재 권한)를 분리해 복구 가능한 fault와 죽여야 하는 fault를 가른다. 버린 대안: 매번 전체 복사(기본 구현) — 단순하지만 `fork`+`exec` 패턴에서 항상 낭비다. 과제 요구 범위 밖이었지만 이 비효율을 없애기로 했다. → [`vm/vm.c`](pintos/vm/vm.c), [설계 노트 §4](docs/design.md)

## 검증

[![CI](https://github.com/woonyong-choi/lrn-pintos/actions/workflows/ci.yml/badge.svg)](https://github.com/woonyong-choi/lrn-pintos/actions/workflows/ci.yml)

| 스위트 | 검증 대상 | 통과/전체 | 실행 명령 |
|---|---|---:|---|
| threads | 스케줄링 · 동기화 · MLFQS · priority donation | **28/28** | `make -C pintos/threads check` |
| userprog | 시스템 콜 · fork/exec/wait · 프로세스 생명주기 | **96/96** | `make -C pintos/userprog check` |
| vm | 페이지 · 프레임 · 스왑 · Copy-on-Write(threads·userprog·filesys 사전조건 포함) | **142/142** | `make -C pintos/vm check` |

수치는 CI(GitHub Actions, `ubuntu-22.04` + `qemu-system-x86`, 2026-09-22 실행, run [`35789337785`](https://github.com/woonyong-choi/lrn-pintos/actions/runs/35789337785))의 개별 job 로그에서 직접 확인한 값입니다. 세 스위트 모두 `FAIL` 0건. userprog는 96/96이며(기존에 적었던 95/95는 이번 재측정에서 정정), vm은 threads·filesys 테스트를 함께 실행하므로 세 수치를 단순 합산하지 않습니다.

## 범위와 한계

교육용 커널로서 범용 OS의 장치 드라이버·보안·다중 코어 지원을 목표로 하지 않습니다. **QEMU 검사 통과가 모든 메모리 압박·스케줄링 조합의 정확성을 보장하지 않습니다** — 위의 두 선점 버그도 전체 스위트를 통과하는 동안 남아 있었습니다. 공유 중인 프레임은 스왑아웃하지 못합니다(역매핑 구조가 없는 한계, [설계 노트 §3](docs/design.md)). swap 슬롯 고갈 경로는 실패 반환은 있으나 그 상태에서 커널이 어떻게 보이는지 테스트로 확인한 적이 없습니다. 성능은 측정한 적이 없고, `ready_list` 비용 표는 복잡도 분석일 뿐 계측값이 아닙니다([설계 노트 §5](docs/design.md)). 파일 시스템 과제(buffer cache, extensible file, subdirectory)는 범위 밖입니다.

## 관련 링크

- [설계 노트](docs/design.md) — 왜 이 자료구조인지, 버린 대안, 알려진 한계
- [단계별 구현 기록](docs/pintos/)
- [담당 범위와 개인 확장 커밋](docs/contribution.md)
- [부모 종료 후 자식 상태 회수 설계](docs/pintos/10-orphan-reaping.md)
- [PintOS Wiki](https://docs.woonyong.com/wiki/pintos/) — 단계별 개념 정리
- [팀 원본 저장소 (Jungle-12-303/wk11_7)](https://github.com/Jungle-12-303/wk11_7)
- [KAIST PintOS 과제 안내](https://casys-kaist.github.io/pintos-kaist/) — 골격 코드와 테스트 하니스 출처
- [CI 실행 기록](https://github.com/woonyong-choi/lrn-pintos/actions)
