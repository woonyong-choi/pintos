# 설계 노트

현재 코드의 자료구조와 동작을 설명한다. 선택 이유는 코드에서 확인할 수 있는 제약과 현재 해석을 구분해 적는다. 단계별 학습 기록은 [README 문서 목록](../README.md#문서)에 있다.

## 우선순위 큐

`ready_list`는 우선순위 내림차순으로 정렬된 이중 연결 리스트다. [list.h](../pintos/include/lib/kernel/list.h)의 각 원소가 `prev`와 `next`를 가진다. [thread.c](../pintos/threads/thread.c)의 `thread_unblock()`과 `thread_yield()`는 `list_insert_ordered()`로 삽입하고, `next_thread_to_run()`은 맨 앞의 원소를 꺼낸다. 리스트 순서가 다음 실행 대상을 결정한다.

| 연산 | 코드에서 분석한 비용 |
|---|---|
| 정렬 삽입 | O(n) |
| 다음 스레드 선택 | O(1) |
| ready 스레드의 우선순위 변경 | 제거 후 재삽입 O(n) |
| MLFQS 일괄 갱신 | O(n) 계산과 O(n log n) 정렬 |

위 값은 실행 시간 측정치가 아니다. n은 대상 목록의 스레드 수다.

### 대안과 선택

`PRI_MIN`부터 `PRI_MAX`까지는 0부터 63까지다. 우선순위별 버킷과 비트맵으로 삽입·선택 비용을 줄이는 대안을 생각할 수 있다. 현재 코드는 정렬 리스트를 유지한다. 버킷을 도입하려면 초기화, MLFQS 갱신, donation 경로의 자료구조를 함께 바꿔야 한다. 실제 비용 차이는 측정하지 않았다.

정렬하지 않고 선택할 때 `list_max()`로 찾는 방법도 있다. 삽입은 O(1), 선택은 O(n)이며 ready 큐의 재정렬 조건을 줄일 수 있다. 현재 구현은 기존의 정렬 규약을 유지하면서 우선순위 변경 경로를 한 함수로 모은다. 두 방법 중 어느 쪽이 빠른지는 실행 패턴과 측정으로 판단해야 한다.

### ready 상태의 lock holder에 기부하는 경우

holder가 ready 큐에 있는 동안 priority donation을 받으면 우선순위 값과 큐 위치를 함께 바꿔야 한다. 값만 바꾸면 맨 앞에서 꺼내는 스케줄러가 더 낮은 우선순위 스레드를 먼저 실행할 수 있다.

[priority-donate-ready.c](../pintos/tests/threads/priority-donate-ready.c)는 이 조건을 재현한다. [테스트 추가](https://github.com/woonyong-choi/pintos/commit/15dacef) 뒤 [수정](https://github.com/woonyong-choi/pintos/commit/4d1cd47)을 적용했다. `thread_set_effective_priority()`가 값 변경과 ready 큐 재배치를 함께 처리하며 `refresh_priority()`와 기부 경로가 이 함수를 사용한다.

MLFQS는 4틱마다 전체 스레드의 우선순위를 갱신한 후 ready 큐를 한 번 정렬한다. 이는 위의 donation 재현과 별개 경로다. `thread_create()`에서 요청 우선순위 대신 실제 `t->priority`를 비교하도록 고친 [선점 수정](https://github.com/woonyong-choi/pintos/commit/b38cc90)은 별도 재현 테스트를 추가한 변경이 아니다.

## MLFQS와 고정소수점

커널은 컨텍스트 전환에서 부동소수점 레지스터를 저장하지 않는다. 4.4BSD 규칙의 감쇠 계수는 정수 나눗셈만으로 표현할 수 없다.

```text
recent_cpu = (2 * load_avg) / (2 * load_avg + 1) * recent_cpu + nice
```

현재 [thread.c](../pintos/threads/thread.c)는 부호를 포함한 32비트 정수에 소수부 14비트를 두는 17.14 형식을 사용한다. 산술 보조 함수도 이 파일에 있으며 별도의 `fixed_point.h`는 없다.

| 형식 | 최소 단위 | 표현 범위 |
|---|---|---|
| 17.14 | 1/16384 | -131072 이상, 131072 미만 |
| 16.16 대안 | 1/65536 | -32768 이상, 32768 미만 |

16.16은 소수부 해상도가 네 배지만 정수 범위가 줄어든다. 현재 구현의 곱셈·나눗셈은 중간 계산을 `int64_t`로 승격한다. 이 선택만으로 모든 입력의 오버플로 안전성이 증명되는 것은 아니다.

매초 `recent_cpu`를 갱신할 때는 ready 스레드뿐 아니라 blocked 스레드도 포함한다. 이를 위해 [thread.h](../pintos/include/threads/thread.h)의 `all_elem`과 전체 스레드 목록을 사용한다. 목록 등록·해제는 인터럽트를 끈 상태에서 처리한다.

MLFQS와 donation은 같은 `priority` 필드를 갱신한다. 두 규칙을 섞지 않도록 [synch.c](../pintos/threads/synch.c)는 `thread_mlfqs`가 켜져 있으면 donation을 건너뛴다.

## 프레임 교체

[vm.c](../pintos/vm/vm.c)의 `vm_get_victim()`은 clock 방식으로 희생 프레임을 고른다.

1. `clock_hand`로 프레임 테이블을 순회한다.
2. `vm_frame_evictable()`이 거짓인 프레임은 건너뛴다.
3. accessed bit가 켜져 있으면 비트를 내리고 다음 프레임으로 간다.
4. 교체 가능하고 accessed bit가 내려가 있으면 희생자로 고른다.

탐색 상한은 `list_size(&frame_table) * 2`다. 모든 프레임이 교체 불가이거나 상한 안에 후보를 찾지 못하면 `NULL`을 반환한다. 두 바퀴를 돈다는 사실만으로 성공이 보장되지는 않는다.

FIFO와 달리 accessed bit를 사용해 최근 접근을 구분한다. 정확한 LRU와 달리 매 접근의 시각을 기록하지 않는다. 이 비교는 동작 차이이며 실제 성능 비교 결과가 아니다.

### 공유 프레임을 제외하는 이유

`struct frame`은 `owner_thread` 하나만 보관하고, `vm_frame_evictable()`은 `ref_count == 1`인 프레임만 후보로 삼는다. 공유 프레임을 내보내려면 모든 공유자의 페이지 테이블 매핑을 찾아 갱신해야 하는데 현재 구조에는 그 역매핑이 없다.

fork로 공유한 프레임이 많으면 메모리 압박 상황에서 후보가 부족해질 수 있다. 이 실패 경로의 별도 검증은 남아 있다. 전체 스위트 통과로 해결됐다고 판단하지 않는다.

## Copy-on-Write

팀 기간에 구현한 COW는 `struct frame.ref_count`로 공유 수를 관리한다. `spt_copy_cow_page()`는 부모와 자식이 같은 프레임을 읽기 전용으로 매핑하게 한다.

쓰기 fault에서 `vm_handle_cow()`는 다음과 같이 처리한다.

- 참조 수가 1보다 크면 새 프레임으로 복사하고 기존 참조 수 감소
- 참조 수가 1이면 복사 없이 쓰기 권한 복원

`page->writable`은 원래 쓰기 권한이고 페이지 테이블의 쓰기 비트는 현재 권한이다. 두 값을 구분해 COW로 일시 제한된 쓰기와 원래 허용되지 않은 쓰기를 처리한다.

`vm_handle_cow()`의 참조 수 판독은 `frame_lock` 밖에 있고, 복사 경로는 새 프레임 확보 뒤 락을 잡아 기존 참조 수를 감소시킨다. [exception.c](../pintos/userprog/exception.c)의 page fault 경로는 인터럽트를 켠다. 따라서 단일 코어라는 이유만으로 중간 선점이나 프레임 수명 문제가 없다고 단정할 수 없다. 경쟁의 재현과 정확성 판정은 별도 검증이 필요하다.

## 검증 범위와 남은 일

전체 시험 수와 실행 근거는 [README 결과](../README.md#결과)에 둔다.

| 항목 | 현재 범위 | 남은 확인 |
|---|---|---|
| 스케줄러 성능 | 복잡도 분석 | 스레드 수별 정렬 비교 횟수와 비용 측정 |
| 공유 프레임 교체 | 공유 중인 프레임은 후보에서 제외 | 메모리 압박 실패 경로와 역매핑 설계 |
| swap 슬롯 고갈 | [anon.c](../pintos/vm/anon.c)의 슬롯 확보 실패 시 `false` 반환 | 상위 경로의 종료·복구·자원 회수 검증 |
| COW 동시성 | QEMU 단일 코어 검사 | 참조 수 판독·복사·해제 중 선점과 수명 검증 |
| 파일 시스템 확장 | 과제 범위 밖 | buffer cache, extensible file, subdirectory 미구현 |
