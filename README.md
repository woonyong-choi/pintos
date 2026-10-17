# PintOS

KAIST PintOS x86-64 골격에 스레드 스케줄링, 사용자 프로세스, 가상 메모리를 구현한 팀 학습 프로젝트입니다. CI에서 threads 28개, userprog 96개, vm 142개 검사를 통과한 기록이 있습니다. 스위트 간 중복이 있어 검사 수를 합산하지 않습니다.

[![CI](https://github.com/woonyong-choi/pintos/actions/workflows/ci.yml/badge.svg)](https://github.com/woonyong-choi/pintos/actions/workflows/ci.yml)

![QEMU에서 threads 검사 6개를 실행하는 기존 녹화](docs/demo.gif)

기존 데모는 `alarm-single`, `alarm-priority`, `priority-change`, `priority-donate-one`, `priority-donate-nest`, `priority-sema`를 실행한 화면입니다. 전체 스위트의 결과는 아래 CI 기록에서 확인할 수 있습니다.

## 목차

- [문제와 제약](#문제와-제약)
- [설계 결정](#설계-결정)
- [구현](#구현)
- [결과](#결과)
- [실행](#실행)
- [문서](#문서)
- [작업과 이력](#작업과-이력)

## 문제와 제약

스케줄러가 높은 우선순위의 스레드를 먼저 실행하고, 사용자 프로세스를 격리하며, 물리 메모리가 부족할 때 페이지를 교체하는 흐름을 구현합니다. 교육용 단일 코어 커널이며 범용 OS의 장치 지원, 보안, 다중 코어 동작을 검증한 프로젝트는 아닙니다.

팀 기간에는 alarm·priority donation과 userprog·vm 전반에 참여했습니다. COW는 팀 기간 공동 작업의 결과입니다. 과제 종료 후에는 MLFQS를 구현하고, `thread_create()`의 선점 판단과 ready 상태 lock holder의 donation 오류를 수정했습니다. 후자는 재현 테스트를 먼저 추가했습니다. 커밋과 파일별 근거는 [담당 범위와 개인 확장 커밋](docs/contribution.md)에 있습니다.

## 설계 결정

| 결정 | 선택과 이유 | 대안과 한계 |
|---|---|---|
| 우선순위 큐 | 이중 연결 리스트를 우선순위 순으로 유지하고 맨 앞에서 선택 | 삽입은 O(n), 선택은 O(1). 버킷과의 성능 차이는 측정 전 |
| 우선순위 변경 | `thread_set_effective_priority()`가 값 변경과 ready 큐 재배치를 함께 처리 | MLFQS는 4틱마다 일괄 재계산 후 정렬 |
| MLFQS | 17.14 고정소수점과 전체 스레드 목록으로 CPU 사용량 갱신 | 커널에서 부동소수점 레지스터를 쓰지 않는 조건 |
| 프레임 교체 | accessed bit를 확인하는 clock 방식 | 공유 프레임은 역매핑이 없어 교체 후보에서 제외 |
| COW fork | 프레임을 읽기 전용으로 공유하고 쓰기 fault에서 복사 | 참조 수가 1이면 복사 없이 쓰기 권한 복원. 동시성 경계는 추가 검증 필요 |

선택 근거, 비교한 대안, 실패 경로는 [설계 노트](docs/design.md)에 있습니다.

## 구현

| 진입점 | 주요 코드 | 역할 |
|---|---|---|
| 타이머 인터럽트 | [thread.c](pintos/threads/thread.c), [synch.c](pintos/threads/synch.c) | 스케줄링, 동기화, MLFQS, priority donation |
| 시스템 콜 | [syscall.c](pintos/userprog/syscall.c), [process.c](pintos/userprog/process.c) | fork·exec·wait와 프로세스 수명 관리 |
| 페이지 fault | [vm.c](pintos/vm/vm.c), [anon.c](pintos/vm/anon.c), [file.c](pintos/vm/file.c) | 지연 적재, 프레임·스왑, mmap, COW |

`pintos/filesys/`는 제공 골격과 팀 작업에 필요한 기본 기능을 사용합니다. buffer cache, extensible file, subdirectory를 구현하는 파일 시스템 과제는 범위 밖입니다.

## 결과

| 스위트 | 확인 결과 | 실행 명령 |
|---|---:|---|
| threads | 28/28 통과 | `make -C pintos/threads check` |
| userprog | 96/96 통과 | `make -C pintos/userprog check` |
| vm | 142/142 통과 | `make -C pintos/vm check` |

위 수치는 [main `00e6ec6`의 CI 실행](https://github.com/woonyong-choi/pintos/actions/runs/36360323335)에서 각 job의 `All N tests passed.` 출력을 확인한 결과입니다. 실행은 2026-09-28에 끝났으며 `ubuntu-22.04`와 `qemu-system-x86`을 사용합니다. vm 스위트에는 threads·userprog·filesys 검사가 포함됩니다. 문서 정리 시점에 새로 측정한 수치는 아닙니다.

검사 통과는 모든 메모리 압박·스케줄링 조합의 정확성을 보장하지 않습니다. 공유 프레임 교체, swap 슬롯 고갈 시 동작, COW 참조 수의 동시성은 추가 검증이 필요합니다. 성능은 측정 전입니다.

## 실행

x86-64 Linux에서 GCC, Make, Perl, QEMU가 필요합니다. macOS나 arm64 환경에서는 저장소의 x86-64 컨테이너를 사용하세요.

```sh
git clone https://github.com/woonyong-choi/pintos.git
cd pintos
docker build --platform linux/amd64 -t pintos-dev .devcontainer/
docker run --rm --platform linux/amd64 -v "$PWD:/w" -w /w pintos-dev \
  bash -lc 'source pintos/activate && make -C pintos/userprog check'
```

Linux 또는 Dev Container 안에서는 저장소 루트에서 실행하세요. CI도 같은 명령을 사용합니다. 각 명령은 커널을 빌드한 뒤 QEMU에서 검사를 실행합니다.

```sh
source pintos/activate
make -C pintos/threads check
make -C pintos/userprog check
make -C pintos/vm check
```

개별 출력과 판정은 각 모듈의 `build/tests/`에 남습니다. `.devcontainer/Dockerfile`은 컨테이너 셸에서 `/workspaces/*/pintos/activate`를 찾습니다. 위처럼 `/w`로 마운트하면 `source pintos/activate`를 직접 실행하세요.

## 문서

문서는 한국어로 작성합니다. 현재 구현과 과거 학습 기록을 구분해 읽으세요. 코드와 상태가 다르면 현재 소스와 CI 결과를 확인하세요.

| 읽을 목적 | 문서 | 범위 |
|---|---|---|
| 현재 선택과 한계 | [설계 노트](docs/design.md) | 자료구조, 스케줄러, 교체, COW |
| 기여와 커밋 추적 | [담당 범위와 개인 확장 커밋](docs/contribution.md) | 팀 기간과 이후 변경 구분 |
| 커널 흐름 이해 | [전체 그림](docs/pintos/01-big-picture.md), [전체 개요](docs/pintos/02-overview.md) | 초기 학습 기록 |
| 리스트와 동기화 | [자료구조](docs/pintos/03-data-structures.md), [동기화](docs/pintos/04-synchronization.md) | 개념과 구현 예시 |
| 스레드 과제 구현 | [Alarm Clock](docs/pintos/05-alarm-clock.md), [Priority Scheduling](docs/pintos/06-priority-scheduling.md), [MLFQS](docs/pintos/07-advanced-scheduler.md), [구현 가이드](docs/pintos/08-implementation-guide.md) | 단계별 학습·구현 안내 |
| 프로세스 과제 추적 | [작업 계획](docs/pintos/w02-01-project2-work-plan.md), [프로세스 테스트 기록](docs/pintos/w02-02-project2-process-tests.md) | 팀 당시 계획과 관측 |
| 부모 종료 후 자식 처리 | [상태 회수 설계](docs/pintos/10-orphan-reaping.md), [설계 비교](docs/pintos/11-orphan-reaping-comparison.md) | 팀 당시 설계와 비교 |
| 개발 환경과 디버깅 | [학습 Q&A](docs/member/woonyong/README.md) | 기존 질의 기록 |

- [KAIST PintOS 과제 안내](https://casys-kaist.github.io/pintos-kaist/)
- [팀 원본 저장소](https://github.com/Jungle-12-303/wk11_7)
- [PintOS Wiki](https://docs.woonyong.com/wiki/pintos/)

## 작업과 이력

[이슈](https://github.com/woonyong-choi/pintos/issues)에서 미해결 조건을, [PR](https://github.com/woonyong-choi/pintos/pulls)에서 변경과 검증 근거를 확인할 수 있습니다. [프로젝트](https://github.com/users/woonyong-choi/projects/31)는 비공개 작업 보드이며 권한이 있는 계정만 볼 수 있습니다. 상태는 대기·진행·검토·보류·완료를 사용합니다.

기존 커밋은 팀 출처와 변경 근거를 보존하기 위해 재작성하지 않습니다. 최근 이력과 파일별 변경은 아래 명령으로 조회하세요.

```sh
git log --oneline --decorate -20
git log --follow -- pintos/threads/thread.c
git log --follow -- pintos/vm/vm.c
```
