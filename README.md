<h1 align="center">PintOS</h1>

<p align="center">스레드 스케줄링·사용자 프로세스·가상 메모리를 구현한 x86-64 교육용 커널</p>

<p align="center">
  <a href="https://github.com/woonyong-choi/pintos/actions/workflows/ci.yml"><img src="https://github.com/woonyong-choi/pintos/actions/workflows/ci.yml/badge.svg" alt="CI"></a>
  <img src="https://img.shields.io/badge/C-555555?logo=c&amp;logoColor=white" alt="C">
  <img src="https://img.shields.io/badge/x86--64-555555" alt="x86-64">
  <img src="https://img.shields.io/badge/QEMU-555555?logo=qemu&amp;logoColor=white" alt="QEMU">
  <img src="https://img.shields.io/badge/Ubuntu-22.04-555555?logo=ubuntu&amp;logoColor=white" alt="Ubuntu 22.04">
</p>

KAIST PintOS를 바탕으로 진행한 팀 학습 프로젝트입니다. 팀 구현에 참여한 뒤, 개인 작업으로 MLFQS를 완성하고 우선순위 선점 오류 두 건을 수정했습니다. COW와 팀 기간 구현의 기여는 [담당 범위](docs/contribution.md)에서 구분합니다.

<details>
<summary>QEMU 테스트 실행 데모</summary>

![QEMU에서 threads 검사 6개가 통과하는 실행 화면](docs/demo.gif)

기존 녹화는 alarm·priority·donation 관련 검사 6개를 실행한 화면입니다. 전체 스위트 결과는 [검증 결과](#결과)에 있습니다.

</details>

## 문제와 제약

높은 우선순위 스레드를 먼저 실행하고, fork·exec·wait로 사용자 프로세스를 관리하며, 물리 메모리가 부족하면 페이지를 교체하는 커널을 구현합니다.

팀 기간에는 alarm·priority donation과 userprog·vm 전반에 참여했습니다. 개인 후속 작업은 미구현 MLFQS의 완성과 스케줄러 결함 수정입니다. 범위는 교육용 단일 코어 환경이며, 파일 시스템 확장 과제인 buffer cache·extensible file·subdirectory는 구현하지 않았습니다.

## 설계 결정

| 다룬 문제 | 적용한 방법 | 확인 근거 |
|---|---|---|
| ready 상태 lock holder가 기부를 받아도 큐 순서가 바뀌지 않는 오류 | 우선순위 값 변경과 ready 큐 재배치를 한 함수로 처리 | [재현 테스트](pintos/tests/threads/priority-donate-ready.c) 추가 후 [수정](https://github.com/woonyong-choi/pintos/commit/4d1cd47) |
| MLFQS에서 새 스레드의 요청값과 실제 우선순위가 달라지는 경우 | `thread_create()`에서 실제 `t->priority`로 선점 판단 | [비교식 수정](https://github.com/woonyong-choi/pintos/commit/b38cc90)과 회귀 검사 |
| 부동소수점 레지스터를 쓰지 않는 커널의 CPU 사용량 계산 | 17.14 고정소수점 연산과 blocked 스레드까지 포함한 전체 목록 갱신 | [MLFQS 구현](https://github.com/woonyong-choi/pintos/commit/3162890) |

ready 큐는 이중 연결 리스트를 우선순위 순으로 유지합니다. 삽입 비용은 O(n), 다음 스레드 선택은 O(1)이며 성능 측정값은 아닙니다. 팀 구현의 clock 교체와 COW는 공유 프레임의 역매핑이 없어 교체 후보가 제한됩니다. 대안과 남은 제약은 [설계 노트](docs/design.md)에 기록했습니다.

## 구현

| 영역 | 구현 범위와 코드 |
|---|---|
| 스레드 | alarm, priority donation, MLFQS. [thread.c](pintos/threads/thread.c), [synch.c](pintos/threads/synch.c) |
| 사용자 프로세스 | 시스템 콜, fork·exec·wait, 자식 상태 회수. [syscall.c](pintos/userprog/syscall.c), [process.c](pintos/userprog/process.c) |
| 가상 메모리 | 지연 적재, 스택 확장, 스왑, mmap, COW. [vm.c](pintos/vm/vm.c), [anon.c](pintos/vm/anon.c), [file.c](pintos/vm/file.c) |

팀 구현과 개인 후속 변경의 커밋별 경계는 [기여 기록](docs/contribution.md)에 있습니다.

## 결과

| 스위트 | 통과 결과 |
|---|---:|
| threads | 28/28 |
| userprog | 96/96 |
| vm | 142/142 |

[main `1847e02`의 CI](https://github.com/woonyong-choi/pintos/actions/runs/39033536043)에서 확인한 결과입니다. 2026-10-17에 Ubuntu 22.04와 QEMU로 실행했습니다. 스위트에 포함된 검사가 겹치므로 수치를 합산하지 않습니다.

현재 결과는 해당 QEMU 검사 범위입니다. [COW 수명](https://github.com/woonyong-choi/pintos/issues/2)과 [swap 고갈 경로](https://github.com/woonyong-choi/pintos/issues/3)의 추가 검증은 남아 있으며 성능은 측정 전입니다.

## 실행

저장소 루트에서 실행하세요. Docker는 x86-64 컨테이너를 사용하므로 arm64 macOS에서도 같은 구성을 선택할 수 있습니다.

```sh
git clone https://github.com/woonyong-choi/pintos.git
cd pintos
docker build --platform linux/amd64 -t pintos-dev .devcontainer/
docker run --rm --platform linux/amd64 -v "$PWD:/w" -w /w pintos-dev \
  bash -lc 'source pintos/activate && make -C pintos/threads check'
```

Linux나 Dev Container 안에서는 GCC, Make, Perl, QEMU를 준비하고 실행하세요. CI도 아래 명령을 사용합니다.

```sh
source pintos/activate
make -C pintos/threads check
make -C pintos/userprog check
make -C pintos/vm check
```

각 명령은 커널을 빌드한 뒤 QEMU에서 검사를 실행합니다. 출력은 모듈별 `build/tests/`에 남습니다.

## 문서

- 설계
  - [설계 노트](docs/design.md)
- 기여
  - [담당 범위와 개인 확장 커밋](docs/contribution.md)
- 개념
  - [PintOS Wiki](https://docs.woonyong.com/wiki/pintos/)

<details>
<summary>팀 당시 학습·구현 기록</summary>

- 커널
  - [전체 그림](docs/pintos/01-big-picture.md)
  - [전체 개요](docs/pintos/02-overview.md)
- 자료구조·동기화
  - [리스트](docs/pintos/03-data-structures.md)
  - [동기화](docs/pintos/04-synchronization.md)
- 스레드
  - [Alarm Clock](docs/pintos/05-alarm-clock.md)
  - [Priority Scheduling](docs/pintos/06-priority-scheduling.md)
  - [MLFQS](docs/pintos/07-advanced-scheduler.md)
  - [구현 가이드](docs/pintos/08-implementation-guide.md)
- 프로세스
  - [작업 계획](docs/pintos/w02-01-project2-work-plan.md)
  - [검사 기록](docs/pintos/w02-02-project2-process-tests.md)
- 자식 상태 회수
  - [초기 설계](docs/pintos/10-orphan-reaping.md)
  - [설계 비교](docs/pintos/11-orphan-reaping-comparison.md)
- 환경·디버깅
  - [학습 Q&A](docs/member/woonyong/README.md)

</details>

[KAIST 과제 안내](https://casys-kaist.github.io/pintos-kaist/) · [팀 원본](https://github.com/Jungle-12-303/wk11_7) · [이슈](https://github.com/woonyong-choi/pintos/issues) · [PR](https://github.com/woonyong-choi/pintos/pulls)
