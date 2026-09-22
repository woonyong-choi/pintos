# lrn-pintos — 스레드 스케줄링과 가상 메모리를 구현한 교육용 커널

KAIST PintOS x86-64 골격 위에 **우선순위 스케줄링·priority donation·MLFQS·시스템 콜·demand paging·Copy-on-Write fork**를 직접 구현한 커널입니다. "스레드가 어떤 순서로 실행되고, 프로세스가 메모리를 어떻게 나눠 쓰고 되돌려주는가"를 코드로 확인하려고 만들었습니다.

![PintOS 테스트 통과 스크린샷](docs/screenshot.png)

## 버전업된 모습

팀 과제가 끝난 시점에 두 가지가 남아 있었습니다. 하나는 **아예 구현되지 않은 MLFQS**, 다른 하나는 **테스트는 통과하는데 실제로는 틀린 선점 코드**였습니다.

MLFQS는 `thread_set_nice`·`thread_get_load_avg`·`thread_get_recent_cpu`가 전부 `/* TODO */` 스텁으로 `0`을 반환하는 상태였습니다.[^stub] 이걸 4BSD 규칙대로 구현하면서 제일 까다로웠던 건 **정수 연산으로는 `recent_cpu` 감쇠 계수를 표현할 수 없다**는 점이었습니다. `(2*load_avg)/(2*load_avg+1)` 같은 값이 정수 나눗셈에서는 0이나 1로 뭉개집니다. 결국 17.14 고정소수점 연산을 따로 만들어 그 위에 올렸습니다. 갱신 대상도 ready 큐가 아니라 **blocked 스레드를 포함한 전체 스레드**여야 했습니다 — 자고 있는 스레드의 `recent_cpu`도 감쇠해야 깨어났을 때 우선순위가 맞기 때문입니다. 그래서 `all_elem`으로 전역 스레드 목록을 따로 유지하게 됐습니다. 마지막으로 MLFQS와 priority donation을 **동시에 켜면 안 된다**는 것(donation이 MLFQS가 계산한 우선순위를 덮어씁니다)을 뒤늦게 알고 `synch.c`에서 `thread_mlfqs`일 때 donation 경로를 통째로 우회하도록 고쳤습니다.

두 번째가 더 뼈아팠습니다. `thread_create()`에서 새 스레드에게 CPU를 양보할지 판단할 때 **호출자가 넘긴 `priority` 인자를 비교하고 있었고, 실제로 스레드에 설정된 `t->priority`를 봐야 했습니다.** 기본 스케줄러에서는 두 값이 같아 증상이 없었지만, MLFQS에서는 생성 직후 우선순위가 다시 계산되므로 엉뚱한 스레드가 CPU를 잡습니다. `priority` → `t->priority`, 한 글자짜리 차이인데 그때까지 통과하던 테스트는 아무것도 잡아주지 못했습니다.

고친 뒤 vm 회귀 전체를 다시 돌려 깨진 것이 없는지 확인했고, 로컬에서만 재현되는 걸 막으려고 x86 QEMU CI를 붙였습니다.

**그리고 같은 종류의 구멍이 하나 더 있었습니다.** 선점 버그를 잡고 나서
"테스트가 통과하는데 틀린 코드가 또 없을까" 를 `ready_list` 기준으로 다시 봤습니다.
`ready_list`는 우선순위 내림차순 **정렬을 불변식으로** 두고 `next_thread_to_run()`이
뒤를 훑지 않고 front만 pop합니다. 그러면 **이미 큐에 들어 있는 스레드의 우선순위를
바꾸는 경로가 전부** 위치까지 옮겨야 하는데, priority donation은 `holder->priority`에
값만 쓰고 있었습니다. lock 소유자가 실행 중이 아니라 ready 상태로 대기 중일 때
기부는 도착하지만 큐 순서는 그대로여서, **donation이 없애려던 priority inversion이
그대로 남습니다.** KAIST의 priority-donate 테스트 8개는 전부 *실행 중인* 스레드에게
기부하기 때문에 하나도 이걸 잡지 못합니다.

이번에는 순서를 바꿔서, **먼저 재현 테스트를 쓰고**
([`priority-donate-ready`](pintos/tests/threads/priority-donate-ready.c))
그게 `false`를 뱉는 걸 확인한 뒤에 고쳤습니다. 고친 방법은 불변식의 소유자를 하나로
만드는 것입니다 — `thread_set_effective_priority()`가 값 쓰기와 큐 위치 이동을 같이
하고, 기부·`refresh_priority` 경로가 전부 이 함수를 거칩니다.

| 항목 | 팀 과제 종료 시점 | 현재 |
|---|---|---|
| MLFQS (`nice`·`recent_cpu`·`load_avg`) | **미구현** — `/* TODO */` 스텁이 `0` 반환[^stub] | **4BSD 규칙 구현**, 17.14 고정소수점 |
| MLFQS 갱신 대상 | — | **전체 스레드(`all_elem`, blocked 포함)** |
| MLFQS + priority donation | — | **MLFQS일 때 donation 경로 우회** |
| `thread_create` 선점 판단 | 인자 `priority` 비교 (MLFQS에서 오동작) | **`t->priority` 비교** |
| ready 상태 lock holder에게 기부 | 값만 덮어써 큐 순서가 깨짐 → inversion 잔존 | **`thread_set_effective_priority()`가 큐 위치까지 이동** |
| 그 버그의 재현 테스트 | 없음 (기존 8개는 *실행 중* 스레드에만 기부) | **`priority-donate-ready` 추가** |
| 설계 근거 문서 | 없음 | **[`docs/design.md`](docs/design.md) — 고른 것·버린 대안·한계** |
| 회귀 검증 | 로컬 수동 | **x86 QEMU GitHub Actions (threads·vm job 분리)** |

## 구동모습

QEMU에서 실제로 `make check`를 실행한 화면입니다(threads 테스트 6개, 2배속 재생). `alarm-single`·`alarm-priority`·`priority-change`·`priority-donate-one`·`priority-donate-nest`·`priority-sema`가 모두 통과해 "All 6 tests passed."가 출력됩니다. `pass` 줄이 두 번 나오는 것은 테스트 실행 중 한 번, `make check`의 요약에서 한 번 출력되기 때문입니다. Apple Silicon에서 Docker(linux/amd64 에뮬레이션)로 돌렸고 재현은 `docker build --load -t pintos-demo -f scripts/Dockerfile.demo scripts && bash scripts/demo_check.sh`입니다. 전체 threads·vm 스위트는 이 GIF에 포함하지 않았고 CI가 실행합니다.

![PintOS 테스트 실행 GIF](docs/demo.gif)

## 메인 기술

![구조도: 스레드 스케줄링, 프로세스, 가상 메모리, Copy-on-Write로 이어지는 경로](docs/figure.png)

**왜 이 자료구조를 골랐고 무엇을 버렸는지는 [설계 노트](docs/design.md)에 정리했습니다.**

- **우선순위 스케줄링과 priority donation** — 높은 우선순위부터 실행하고, lock 대기 시 소유자에게 우선순위를 넘겨 priority inversion을 막습니다. 중첩 donation과 다중 lock 보유를 처리합니다. `ready_list`는 정렬을 불변식으로 두고 `next_thread_to_run()`이 front만 pop하므로, 기부가 일어나면 값뿐 아니라 큐 위치까지 옮겨야 합니다 — `thread_set_effective_priority()`가 그 책임을 혼자 집니다. → [`threads/thread.c`](pintos/threads/thread.c), [`threads/synch.c`](pintos/threads/synch.c), [설계 노트 §1](docs/design.md)
- **4BSD MLFQS (17.14 고정소수점)** — `nice`·`recent_cpu`·`load_avg`로 우선순위를 주기적으로 재계산합니다. 1초마다 `load_avg`와 전체 스레드의 `recent_cpu`를, 4틱마다 우선순위를 갱신합니다. 17.14를 고른 이유와 16.16을 버린 이유는 [설계 노트 §2](docs/design.md). → [`threads/thread.c`](pintos/threads/thread.c)
- **Copy-on-Write fork** — fork 시 프레임을 읽기 전용으로 공유하다 쓰기 page fault에서 복사합니다. 참조가 하나만 남으면 복사 없이 쓰기 권한만 돌려줍니다. `page->writable`(원래 권한)과 pml4 쓰기 비트(현재 권한)를 분리해 복구 가능한 fault와 죽여야 하는 fault를 가릅니다. → [`vm/vm.c`](pintos/vm/vm.c), [설계 노트 §4](docs/design.md)
- **accessed bit를 쓰는 clock 프레임 교체** — `clock_hand`가 프레임 테이블을 원형으로 돌며 `pml4_is_accessed()`가 켜진 프레임은 비트를 내리고 한 번 봐줍니다(second chance). swap-out 시 소유자의 `pml4`에서 해당 페이지를 함께 내려 stale mapping을 남기지 않습니다. **COW로 공유 중인 프레임(`ref_count > 1`)은 후보에서 제외되는데, 이건 정책이 아니라 역매핑이 없어서 생긴 제약입니다** — 자세한 이유는 [설계 노트 §3](docs/design.md). → [`include/vm/vm.h`](pintos/include/vm/vm.h), [`vm/vm.c`](pintos/vm/vm.c), swap 연산은 [`vm/anon.c`](pintos/vm/anon.c)
- **Demand paging · mmap · stack growth** — 접근 시점에 페이지를 채우고, 파일 backed 페이지와 스택 확장을 page fault 경로에서 구분합니다. → [`vm/file.c`](pintos/vm/file.c), [`vm/vm.c`](pintos/vm/vm.c)
- **프로세스 수명 관리** — fork·exec·wait, 부모·자식 종료 순서, **부모가 먼저 죽은 자식(orphan)의 상태 회수**를 두 개의 세마포어로 처리합니다. → [`userprog/process.c`](pintos/userprog/process.c), [설계 노트](docs/pintos/10-orphan-reaping.md)

## 계획

- **`list_insert_ordered` 비교 횟수를 계측**해 "스레드 수가 이 정도면 정렬 리스트로 충분하다"를 숫자로 만든다. 지금 그 주장의 근거는 복잡도 분석뿐이다([설계 노트 §5](docs/design.md)).
- 프레임마다 `(thread, va)` **역매핑**을 붙여 공유 중인 프레임까지 교체 대상으로 넓힌다. 현재 `ref_count == 1` 제한은 정책이 아니라 이 구조가 없어서 생긴 제약이다.
- swap 슬롯 고갈 경로를 테스트로 재현한다. 실패 반환은 있지만 그때 커널이 어떻게 보이는지 확인한 적이 없다.
- 파일 시스템 과제(buffer cache, extensible file, subdirectory)를 이어서 구현한다.

## 링크

- [설계 노트](docs/design.md) — 왜 이 자료구조인지, 버린 대안, 알려진 한계
- [PintOS Wiki](https://docs.woonyong.com/wiki/pintos/) — 단계별 개념 정리
- [단계별 구현 기록](docs/pintos/)
- [부모 종료 후 자식 상태 회수 설계](docs/pintos/10-orphan-reaping.md)
- [팀 원본 저장소 (Jungle-12-303/wk11_7)](https://github.com/Jungle-12-303/wk11_7)
- [KAIST PintOS 과제 안내](https://casys-kaist.github.io/pintos-kaist/)
- [CI 실행 기록](https://github.com/woonyong-choi/lrn-pintos/actions)

## 담당

**크래프톤 정글 팀 과제(원본: [Jungle-12-303/wk11_7](https://github.com/Jungle-12-303/wk11_7))에서 시작했고, 종료 후 개인 저장소에서 계속 수정·학습·확장하고 있습니다.**

| 항목 | 내용 |
|---|---|
| 원본 팀 저장소 | [Jungle-12-303/wk11_7](https://github.com/Jungle-12-303/wk11_7) |
| 팀 과제 기간 | 2026-04-23 ~ 2026-05-21 (커밋 `581a1e4` ~ `7aa964b`) |
| 팀 구성 | 7인. `main` 에 병합된 기여자는 6인이고, 나머지 1인의 커밋은 병합되지 않은 브랜치에 남아 있다 — `git shortlog -sne --all` 로 확인 |
| **본인 담당** | **팀 기간 커밋 159/214개(약 74%, `main` 기준)로 최다 기여. threads(alarm·priority·donation)와 userprog·vm 전반.** MLFQS는 팀 기간에 미구현으로 남아 종료 후 개인 작업으로 구현했다 |

기반 커널·테스트 하니스는 KAIST 과제 제공물입니다. 팀 코드 전체를 개인 구현으로 주장하지 않으며, 팀 구현과 개인 변경은 커밋 저자와 파일별 diff로 구분합니다.

### 개인 확장 (팀 과제 종료 후)

**커밋 범위: [`a1ff596`](https://github.com/woonyong-choi/lrn-pintos/commit/a1ff596) (2026-08-02) ~ 현재, 21개 커밋.** `git log --author` 기준입니다.[^authors]

| 무엇이 달라졌나 | 커밋 | 파일 |
|---|---|---|
| **MLFQS 구현**(팀 종료 시점에는 TODO 스텁) — 4BSD 규칙, 17.14 고정소수점, 전체 스레드 갱신용 `all_elem` 목록, MLFQS일 때 donation 우회 | [`3162890`](https://github.com/woonyong-choi/lrn-pintos/commit/3162890) | `thread.c` (+188), `thread.h` (+3), `synch.c` (+9) |
| **선점 판단 버그 수정** — `thread_create`에서 요청값 대신 실제 `t->priority` 비교. vm 141개 회귀 재검증 | [`b38cc90`](https://github.com/woonyong-choi/lrn-pintos/commit/b38cc90) | `thread.c` |
| **x86 QEMU CI 도입** — threads·vm job 분리, ubuntu-22.04 + `qemu-system-x86` | [`7db77ff`](https://github.com/woonyong-choi/lrn-pintos/commit/7db77ff) | `.github/workflows/ci.yml` |
| 저장소 정리 — 중복 문서 제거, 커널 범위 명확화, 출처·기여 경계 명시 | [`a1ff596`](https://github.com/woonyong-choi/lrn-pintos/commit/a1ff596), [`81d907b`](https://github.com/woonyong-choi/lrn-pintos/commit/81d907b), [`7286e4c`](https://github.com/woonyong-choi/lrn-pintos/commit/7286e4c) | `docs/`, `README.md` |
| 검증된 기여 수치 정정 | [`31e6d75`](https://github.com/woonyong-choi/lrn-pintos/commit/31e6d75) | `README.md` |
| **ready 상태 lock holder 기부 버그 수정** — 재현 테스트를 먼저 쓰고 고쳤다. 불변식 소유자를 `thread_set_effective_priority()` 하나로 통일 | [`15dacef`](https://github.com/woonyong-choi/lrn-pintos/commit/15dacef), [`4d1cd47`](https://github.com/woonyong-choi/lrn-pintos/commit/4d1cd47) | `priority-donate-ready.c` (신규), `thread.c`, `synch.c` |
| **설계 근거 문서화** — 고른 자료구조·버린 대안·알려진 한계 | [`2ec378a`](https://github.com/woonyong-choi/lrn-pintos/commit/2ec378a) | `docs/design.md` (신규), `README.md` |
| 죽은 코드·유령 파일 제거 — 참조 없는 스크립트·이미지·문서, 구현이 끝난 자리에 남은 TODO | [`e820dc4`](https://github.com/woonyong-choi/lrn-pintos/commit/e820dc4) | `vm.c`, `synch.c`, `docs/`, `scripts/` |

팀 과제 종료 시점(`7aa964b`) 대비 커널 코드 순증은 `pintos/` 기준 +381/-100행입니다(`git diff --stat 7aa964b HEAD -- pintos/`).

## 구동방법

x86-64 Linux, GCC, Make, Perl, QEMU가 필요합니다. **macOS(arm64)에서는 저장소의 x86-64 Dev Container를 씁니다.**

```bash
git clone https://github.com/woonyong-choi/lrn-pintos.git
cd lrn-pintos
```

컨테이너 안(또는 x86-64 Linux)의 Bash에서 저장소 루트에서 실행합니다.

```bash
source pintos/activate
make -C pintos/threads check     # 스레드 스케줄링
make -C pintos/userprog check    # 사용자 프로그램
make -C pintos/vm check          # 가상 메모리
```

Docker만 있다면 Dev Container를 직접 띄워도 됩니다.

```bash
docker build --platform linux/amd64 -t pintos-dev .devcontainer/
docker run --rm --platform linux/amd64 -v "$PWD:/w" -w /w pintos-dev \
  bash -lc 'source pintos/activate && make -C pintos/threads check'
```

세 검사 모두 [CI](.github/workflows/ci.yml)의 개별 job으로 매 push·PR마다 실행합니다.

| 검사 | 명령 | CI job | 최근 측정 |
| --- | --- | --- | --- |
| 스레드 | `make -C pintos/threads check` | `threads` | 28개 통과 (아래 검증 절) |
| 사용자 프로그램 | `make -C pintos/userprog check` | `user-programs` | 95/95 통과 |
| 가상 메모리 | `make -C pintos/vm check` | `virtual-memory` | 142개 통과 (아래 검증 절) |

사용자 프로그램 95/95는 x86-64 Dev Container(`.devcontainer/Dockerfile`, Ubuntu 22.04 + `qemu-system-x86`)에서 측정한 값입니다.

`activate`는 저장소의 `pintos/utils`를 PATH에 추가합니다. 각 명령은 커널을 빌드하고 QEMU에서 검사를 실행하며, 개별 실행 출력과 판정은 해당 디렉터리의 `build/tests/`에 남습니다. 단계별 구현 기록은 [`docs/pintos/`](docs/pintos/)를 참조합니다.

## 스펙

| 구분 | 내용 |
|---|---|
| 언어 | C (GNU C, 커널 모드 freestanding) + x86-64 어셈블리 |
| 대상 아키텍처 | x86-64 |
| 골격 | [KAIST PintOS](https://casys-kaist.github.io/pintos-kaist/) (과제 제공물) |
| 빌드 | GNU Make, GCC, Perl (테스트 하니스) |
| 실행·검증 | QEMU (`qemu-system-x86`) |
| 개발 환경 | Dev Container — Ubuntu 22.04 linux/amd64 |
| CI | GitHub Actions `ubuntu-22.04`, threads·userprog·vm job 분리 |

## 검증

[![CI](https://github.com/woonyong-choi/lrn-pintos/actions/workflows/ci.yml/badge.svg)](https://github.com/woonyong-choi/lrn-pintos/actions/workflows/ci.yml)

**threads 28개, vm 142개 전부 통과**합니다. (vm 스위트는 threads 테스트를 함께 돌리므로 두 수치는 겹칩니다.)

| 스위트 | 통과 | 명령 |
|---|---:|---|
| threads | **28/28** | `make -C pintos/threads check` |
| vm | **142/142** | `make -C pintos/vm check` |

두 스위트 모두 **2026-09-22 로컬 Docker(linux/amd64)에서 현재 트리로 직접 재실행**해 확인했습니다.[^run] 아래 "구동방법" 의 명령을 **비어 있는 컨테이너와 새 체크아웃에서 그대로 복사해** 돌린 결과입니다.

```bash
source pintos/activate
make -C pintos/threads check     # All 28 tests passed.
make -C pintos/vm check          # All 142 tests passed.
```

### 현재 범위와 한계

교육용 커널로서 범용 OS의 장치 드라이버·보안·다중 코어 지원을 목표로 하지 않습니다. **QEMU 검사 통과가 모든 메모리 압박·스케줄링 조합의 정확성을 보장하지 않습니다** — 실제로 위의 버그 두 개는 모두 전체 스위트를 통과하는 동안 남아 있었습니다 — 두 번째 것은 `priority-donate-ready`를 새로 쓰기 전까지 어떤 테스트도 건드리지 않던 경로였습니다. swap은 참조 수가 1인 프레임만 고릅니다 — 정책이 아니라 프레임에서 역으로 매핑을 찾을 구조가 없어서 생긴 제약이고, 이유는 [설계 노트 §3](docs/design.md)에 적었습니다. 파일 시스템 과제(buffer cache, extensible file, subdirectory)는 범위 밖입니다.

팀 원본과 비교할 때는 저장소 이름뿐 아니라 커밋을 함께 확인합니다.

## 참고자료

- [KAIST PintOS 과제 안내](https://casys-kaist.github.io/pintos-kaist/) — 골격 코드와 테스트 하니스 출처
- [설계 노트](docs/design.md) · [단계별 구현 기록](docs/pintos/) · [부모 종료 후 회수 설계](docs/pintos/10-orphan-reaping.md)
- [팀 원본 저장소 Jungle-12-303/wk11_7](https://github.com/Jungle-12-303/wk11_7)
- Operating System Concepts (Silberschatz) — MLFQS·priority inversion
- [커널 라이선스](pintos/LICENSE)

[^authors]: `git log --author='woonyong' --since=2026-06-01 --reverse --format='%h %ad %s' --date=short`
[^stub]: `git show 3162890^:pintos/threads/thread.c` 의 `thread_set_nice`·`thread_get_nice`·`thread_get_load_avg`·`thread_get_recent_cpu`.
[^run]: `docker build --platform linux/amd64 -t pintos-dev .devcontainer/` 로 이미지를 새로 만들고, `git archive HEAD` 로 뽑은 새 체크아웃을 `docker run --rm --platform linux/amd64 -v "$PWD:/w" -w /w pintos-dev bash -lc "source pintos/activate && make -C pintos/threads check && make -C pintos/vm check"` → `All 28 tests passed.` / `All 142 tests passed.`
