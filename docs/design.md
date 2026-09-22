# 설계 노트 — 무엇을 왜 그렇게 골랐나

이 문서는 "어떻게 동작하는가"가 아니라 **"왜 이 자료구조인가, 무엇을 버렸는가,
지금 어디가 약한가"** 를 적는다. 동작 설명은 [`docs/pintos/`](pintos/) 의 단계별
기록에 있다.

---

## 1. ready_list — 정렬된 연결 리스트

### 무엇을 골랐나

`ready_list` 는 **우선순위 내림차순으로 항상 정렬된 단일 연결 리스트**다.
[`threads/thread.c`](../pintos/threads/thread.c) 에서 `thread_unblock()` 과
`thread_yield()` 가 `list_insert_ordered()` 로 넣고,
`next_thread_to_run()` 은 **뒤를 훑지 않고 front 하나만 pop** 한다.

즉 **리스트의 순서가 곧 스케줄링 결정**이다. 이건 편의가 아니라 불변식이다.

| 연산 | 비용 |
|---|---|
| `thread_unblock` / `thread_yield` 삽입 | O(n) 비교 |
| `next_thread_to_run` | O(1) |
| 기부로 인한 우선순위 변경 | O(n) — 제거 후 재삽입 |
| MLFQS 4틱 일괄 재계산 | O(n log n) `list_sort` 1회 |

### 버린 대안 ①: 64개 우선순위 버킷 (4.4BSD 원본 방식)

PRI_MIN..PRI_MAX 는 0..63 로 **범위가 작고 고정**이다. 버킷 배열 + 비어있지
않은 최고 버킷 비트맵이면 삽입·선택 모두 O(1) 이고, MLFQS 재계산도
"버킷 간 이동" 으로 끝난다. 4.4BSD 가 실제로 쓰는 구조다.

**버린 이유**: 이 커널은 단일 코어에서 많아야 수십 개 스레드를 돌린다.
O(n) 의 n 이 20 근처라면 삽입 비용 차이는 측정 가능한 수준이 아니고,
버킷 방식은 (a) 64개 리스트 + 비트맵 초기화, (b) MLFQS 재계산 시 전 스레드의
버킷 재배치, (c) priority donation 시 버킷 이동 — 세 군데에서 **일관성을 지킬
지점이 세 배로 늘어난다.** 아래 §1.4 에서 보듯 **지점이 하나일 때도 놓쳤다.**

**되돌릴 조건**: 스레드 수가 수백 단위로 올라가거나 멀티코어로 가면 정렬 삽입의
O(n) 이 인터럽트 컨텍스트에서 문제가 된다. 그때는 버킷이 맞다.

### 버린 대안 ②: 정렬하지 않고 `list_max()` 로 매번 최대값 선택

삽입 O(1), 선택 O(n). 총 비용은 정렬 삽입과 대칭이고, **기부로 우선순위가 바뀌어도
재정렬할 필요가 없다** — 즉 §1.4 의 버그가 구조적으로 불가능하다.

**버린 이유**: 이미 정렬 삽입으로 구현된 코드를 뒤집는 변경이고, `sema->waiters` ·
`cond->waiters` · `donation_list` 도 같은 정렬 규약을 쓰고 있어서 한 곳만 바꾸면
규약이 갈라진다. 불변식을 없애는 대신 **불변식의 소유자를 하나로 만드는 쪽**을
택했다(§1.4).

**솔직한 평가**: 다시 처음부터 짠다면 이쪽이 더 낫다. 정렬 불변식은 공짜가
아니었다.

### 1.4 이 선택이 실제로 만든 버그

정렬 불변식은 **값을 쓰는 모든 경로**가 지켜야 한다. `priority donation` 은
`lock_acquire()` 에서 **lock holder 의 우선순위를 올린다.** 그런데 holder 가
실행 중이 아니라 **ready_list 에 앉아 있을 수 있다.**

```
main(31)  lock A 획득 → mid 에게 선점당해 ready_list 로
ready_list: [ mid(33), main(31) ]
donor(35) → lock_acquire(A) → main 에게 35 기부
ready_list: [ mid(33), main(35) ]   ← 정렬이 깨졌다
next_thread_to_run() → front = mid(33)
```

기부는 도착했는데 **스케줄러는 여전히 낮은 스레드를 고른다.**
donation 이 없애려던 priority inversion 이 그대로 남는다.

KAIST 의 priority-donate 테스트 8개는 전부 **실행 중인** 스레드에게 기부한다.
그래서 27개 테스트를 다 통과하는 동안 이 구멍은 보이지 않았다.
README 가 언급하는 `thread_create` 선점 버그와 **정확히 같은 종류**다.

**재현 테스트**: [`tests/threads/priority-donate-ready`](../pintos/tests/threads/priority-donate-ready.c)
— 기부된 우선순위(35)와 "낮은 스레드보다 먼저 스케줄되었는가" 를 한 줄씩 출력한다.
고치기 전에는 `false`, 고친 뒤에는 `true`.

**고친 방법**: 불변식의 소유자를 하나로 만들었다.
`thread_set_effective_priority()` 가 값 쓰기와 큐 위치 이동을 **같이** 하고,
`refresh_priority()` 와 `lock_acquire()` 의 기부 두 곳이 전부 이 함수를 거친다.
MLFQS 만 예외로 `thread_tick()` 의 `list_sort()` 를 쓴다 — 4틱마다 **전원**의
우선순위가 바뀌므로 개별 재삽입 n번(O(n²))보다 정렬 1회가 싸다.

---

## 2. MLFQS 17.14 고정소수점

### 왜 고정소수점인가

커널은 FPU 를 쓰지 않는다(컨텍스트 전환 때 FP 레지스터를 저장하지 않으므로).
그런데 4.4BSD 규칙의 핵심 계수가 1 보다 작다:

```
recent_cpu = (2*load_avg)/(2*load_avg + 1) * recent_cpu + nice
```

`load_avg = 1` 이면 계수는 `2/3`. 정수 나눗셈으로는 **0** 이 되어 `recent_cpu` 가
매초 통째로 날아가고, 올림하면 **1** 이 되어 영원히 감쇠하지 않는다. 둘 다 틀린다.
그래서 소수부가 반드시 필요하다.

### 왜 17.14 인가

`int` 32비트를 17비트 정수부 + 14비트 소수부로 쓴다.

- **소수부 14비트**: 해상도 `1/16384 ≈ 6.1e-5`. 60초 지수 이동평균
  (`59/60 ≈ 0.9833`) 을 반복 곱할 때 누적 오차가 테스트 허용 범위 안에 든다.
- **정수부 17비트**: `recent_cpu` 는 최악의 경우 계속 CPU 를 잡은 스레드에서
  수천까지 오른다. 17비트(±65535)면 충분하고 여유도 있다.
- 곱셈·나눗셈은 `int64_t` 로 승격한 뒤 되돌린다
  (`((int64_t) x) * y / FP_FACTOR`). 이걸 안 하면 `x * y` 가 32비트에서 넘친다.

**버린 대안: 16.16** — 소수부 해상도는 두 배 좋아지지만 정수부가 ±32767 로
줄어든다. `recent_cpu` 의 상한을 보장할 근거가 없어서 오버플로 위험을 늘리면서
얻는 게 "이미 충분한 해상도를 더 늘리는 것" 뿐이다.

### 왜 `all_list` 가 필요했나

매초 `recent_cpu` 를 감쇠시킬 대상은 **ready 스레드가 아니라 전체 스레드**다.
`sleep_list` 에서 자고 있거나 락을 기다리며 blocked 된 스레드도 감쇠해야,
깨어났을 때의 우선순위가 "그동안 CPU 를 안 썼다" 를 반영한다.
ready_list 만 돌면 오래 잔 스레드가 깨어나자마자 옛날 `recent_cpu` 로 불이익을
받는다. 그래서 `all_elem` 으로 전역 목록을 따로 유지한다
([`thread.h`](../pintos/include/threads/thread.h) `struct thread`).

`all_list` 는 타이머 인터럽트에서 순회하므로 등록·해제를 인터럽트 끄고 한다.

### 왜 MLFQS 에서 donation 을 꺼야 하나

MLFQS 는 `priority` 를 수식으로 **매 4틱 덮어쓴다.** 기부가 같은 필드를 쓰면
둘 중 늦게 쓴 쪽이 이긴다 — 즉 기부가 4틱 안에 지워지거나, 기부값이 수식 결과를
덮어써서 nice 가 무시된다. 어느 쪽이든 두 정책이 서로를 망가뜨린다.
`lock_acquire()` · `lock_release()` 가 `thread_mlfqs` 일 때 기부 경로를 통째로
건너뛴다([`synch.c`](../pintos/threads/synch.c)).

---

## 3. 프레임 교체 — accessed bit 를 쓰는 clock

### 실제 정책

[`vm_get_victim()`](../pintos/vm/vm.c) 은 **clock(second-chance) 알고리즘**이다.
`clock_hand` 가 `frame_table` 을 원형으로 돌면서:

1. 교체 불가 프레임은 건너뛴다(`vm_frame_evictable()`).
2. `pml4_is_accessed()` 가 참이면 → accessed bit 를 내리고 한 번 봐준다.
3. accessed bit 가 내려가 있으면 → 희생자로 고른다.

한 바퀴로 못 고르면 두 바퀴까지 돈다(`max_scans = size * 2`). 첫 바퀴에서
내린 accessed bit 덕분에 두 번째 바퀴는 반드시 후보를 찾는다.

**버린 대안: FIFO** — 링크드 리스트 front 를 그냥 빼면 구현이 절반이지만,
"방금 참조한 페이지" 와 "오래 안 쓴 페이지" 를 구분하지 못해 hot page 를 내보내고
곧바로 다시 읽어들인다. `pml4_is_accessed()` 는 이미 MMU 가 공짜로 채워주는
정보라 쓰지 않을 이유가 없었다.

**버린 대안: 정확한 LRU** — 접근 시각을 유지하려면 매 접근마다 갱신이 필요한데,
그 훅이 page fault 경로에만 있다. 이미 매핑된 페이지의 접근은 fault 를 내지
않으므로 커널이 볼 수 없다. 하드웨어 accessed bit 가 줄 수 있는 근사가 한계고,
clock 이 그 근사를 쓰는 표준적인 방법이다.

### `ref_count == 1` 필터는 정책이 아니라 한계다

`vm_frame_evictable()` 은 **COW 로 공유 중인 프레임(`ref_count > 1`)을 후보에서
뺀다.** 이유는 단순하다: `struct frame` 이 `owner_thread` **하나**만 들고 있어서,
공유 프레임을 swap-out 할 때 **나머지 공유자들의 `pml4` 를 찾아 내릴 방법이 없다.**
하나만 내리면 나머지에는 없어진 프레임을 가리키는 stale mapping 이 남는다.

이건 "참조 수 기반 교체 정책" 이 아니다. **역참조 구조가 없어서 생긴 제약**이다.
제대로 풀려면 프레임마다 `(thread, va)` 역매핑 리스트가 필요하고, 그게 곧
reverse mapping 이다.

**결과**: 메모리 압박이 심하고 fork 된 프로세스가 많으면 교체 후보가 말라서
`vm_get_frame()` 이 NULL 을 반환할 수 있다. 현재 141개 vm 테스트는 이 지점을
건드리지 않는다.

---

## 4. Copy-on-Write

`struct frame` 의 `ref_count` 가 공유 수를 센다.
`fork` 는 부모의 프레임을 복사하지 않고 **양쪽 pml4 를 읽기 전용으로 매핑**하고
`ref_count++` 한다([`spt_copy_cow_page()`](../pintos/vm/vm.c)).

쓰기 fault 가 나면 `vm_handle_cow()` 가 두 갈래로 나뉜다:

- `ref_count > 1` → 새 프레임에 복사하고 원본 `ref_count--`
- `ref_count == 1` → **복사 없이 쓰기 권한만 돌려준다.** 마지막 공유자가 남았으면
  복사할 이유가 없다. 이 경로가 없으면 fork 후 한쪽이 먼저 전부 종료한 뒤에도
  남은 쪽이 매 페이지를 한 번씩 헛복사한다.

`page->writable` 은 **원래 권한**이고 pml4 의 쓰기 비트는 **현재 권한**이다.
둘을 분리해야 "원래 쓰기 가능하지만 지금은 COW 때문에 막힌 페이지" 와
"애초에 읽기 전용인 페이지" 를 구분할 수 있다. 전자는 복구 가능한 fault,
후자는 프로세스를 죽여야 하는 fault 다
(`vm_handle_write_protect_fault()` vs `vm_handle_unwritable_spt_page()`).

---

## 5. 지금 알고 있는 한계와 다음 병목

정직하게, 이 저장소의 범위 안에서 **아직 못 한 것**이다.

1. **성능을 측정한 적이 없다.** 위 §1 의 비용 표는 코드를 읽고 쓴 복잡도이지
   측정값이 아니다. 의미 있는 측정이 되려면 (a) QEMU 에뮬레이션이 아닌
   결정적인 tick 카운트 기준, (b) 스레드 수를 20 → 200 으로 늘렸을 때의
   `list_insert_ordered` 비교 횟수 계측이 필요하다. 그 계측 훅이 지금 없다.
   **"정렬 리스트로 충분하다" 는 주장은 현재 근거가 복잡도 분석뿐이다.**
2. **공유 프레임을 교체하지 못한다** (§3). reverse mapping 이 없는 한 구조적이다.
3. **swap 슬롯 고갈 경로가 테스트되지 않았다.** `anon_swap_out()` 은
   `bitmap_scan_and_flip()` 실패 시 `false` 를 반환하고 그게 위로 전파되지만,
   그 상태에서 커널이 어떻게 보이는지 확인한 적이 없다.
4. **동시성 검증이 QEMU 단일 코어 기준이다.** `frame_lock` 이 `ref_count` 를
   보호하지만, `vm_handle_cow()` 는 락 밖에서 `ref_count > 1` 을 읽고
   락 안에서 감소시킨다. 단일 코어 + 커널 선점 모델에서는 그 사이에
   다른 스레드가 끼어들 창이 page fault 경로에 없어서 현재는 안전하지만,
   **락 경계가 값을 읽는 지점까지 덮고 있지 않다는 것 자체가 약하다.**
5. **파일 시스템 과제(buffer cache, extensible file, subdirectory)는 범위 밖이다.**
