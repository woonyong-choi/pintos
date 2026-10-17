# 담당 범위와 개인 확장 커밋

[README](../README.md)의 기여 설명을 커밋과 파일 변경으로 확인하기 위한 기록이다. 팀 결과와 과제 종료 후 변경을 구분한다.

## 팀 과제

| 항목 | 근거와 범위 |
|---|---|
| 원본 | [Jungle-12-303/wk11_7](https://github.com/Jungle-12-303/wk11_7) |
| 기간 | 2026-04-23부터 2026-05-21까지. 현재 이력의 첫 커밋 `581a1e4`부터 팀 종료 `7aa964b`까지 |
| 전체 이력 | `git rev-list --count 7aa964b` 기준 조상 커밋 198개. 병합 커밋 포함 |
| 사용자 저자 이력 | `git rev-list --count --author='woonyong.kr@gmail.com' 7aa964b` 기준 145개 |
| 참여 범위 | alarm·priority donation, userprog·vm 전반의 구현·통합 |
| 다른 팀원과 공동 작업 | 파일 시스템 콜, 프레임 테이블, 교체, mmap, COW |

기반 커널과 테스트 하니스는 KAIST 제공물이다. 위 수치는 현재 Git 이력을 대상으로 저자 이메일을 묶어 센 결과다. 커밋 수는 코드량, 기능 소유권, 기여율을 나타내지 않는다. 기존의 159/214개와 과제 종료 후 개인 COW 구현이라는 설명은 현재 이력과 맞지 않아 정정했다.

COW는 팀 기간에 이미 존재한다. 예를 들어 [`767f006`](https://github.com/woonyong-choi/pintos/commit/767f006)은 2026-05-21에 팀원 저자로 기록된 공유 페이지 처리 변경이다. 다른 브랜치에도 COW 작업이 있으므로 하나의 커밋 저자만으로 전체 구현을 한 사람에게 귀속하지 않는다. 팀 코드 전체를 개인 구현으로 설명하지 않는다.

`main`과 `--all`은 포함하는 브랜치가 다르다. 브랜치별 작업을 합쳐 현재 main에 반영된 기여로 계산하지 않는다. 같은 사람의 다른 이름 표기는 이메일과 diff를 함께 대조한다.

## 과제 종료 후 변경

아래 기록의 끝점은 [`00e6ec6`](https://github.com/woonyong-choi/pintos/commit/00e6ec6), 2026-09-28로 고정한다. `7aa964b..00e6ec6`에는 27개 커밋이 있다. 첫 후속 커밋은 [`a1ff596`](https://github.com/woonyong-choi/pintos/commit/a1ff596), 2026-08-02다. 이후의 문서 정리 커밋은 이 고정 범위의 개수에 포함하지 않는다.

| 변경 | 커밋과 확인 범위 |
|---|---|
| MLFQS 구현 | [`3162890`](https://github.com/woonyong-choi/pintos/commit/3162890). 고정소수점, 전체 스레드 갱신, donation 우회. 팀 종료 시점의 관련 함수는 스텁 |
| 생성된 스레드의 선점 비교 수정 | [`b38cc90`](https://github.com/woonyong-choi/pintos/commit/b38cc90). 요청 인자 대신 실제 `t->priority` 비교. 새 재현 테스트 추가는 없음 |
| ready 상태 holder의 donation 수정 | [`15dacef`](https://github.com/woonyong-choi/pintos/commit/15dacef)에서 재현 테스트 추가 후 [`4d1cd47`](https://github.com/woonyong-choi/pintos/commit/4d1cd47)에서 큐 재배치 수정 |
| x86 QEMU CI | [`7db77ff`](https://github.com/woonyong-choi/pintos/commit/7db77ff)에서 threads·vm, [`e80c36b`](https://github.com/woonyong-choi/pintos/commit/e80c36b)에서 userprog job 추가 |
| 설계와 코드 정리 | [`2ec378a`](https://github.com/woonyong-choi/pintos/commit/2ec378a)의 설계 설명, [`e820dc4`](https://github.com/woonyong-choi/pintos/commit/e820dc4)의 사용하지 않는 코드·자료 정리 |
| 실행 자료와 검증 근거 | [`3624759`](https://github.com/woonyong-choi/pintos/commit/3624759), [`25115a7`](https://github.com/woonyong-choi/pintos/commit/25115a7)의 녹화·그림, [`426c648`](https://github.com/woonyong-choi/pintos/commit/426c648)의 검사 수 정정 |
| 소개와 기여 설명 정리 | [`a1ff596`](https://github.com/woonyong-choi/pintos/commit/a1ff596)부터 [`00e6ec6`](https://github.com/woonyong-choi/pintos/commit/00e6ec6)까지의 문서 변경. 전체 목록은 아래 명령으로 확인 |

기능을 직접 바꾼 커밋과 문서·CI 커밋을 같은 구현량으로 계산하지 않는다. COW를 과제 종료 후 새로 구현한 항목에 넣지 않는다.

## 이력 확인

저장소 루트에서 실행한다. 날짜 필터나 변하는 HEAD 대신 위의 경계를 고정한다.

```sh
git rev-list --count 7aa964b
git rev-list --count --author='woonyong.kr@gmail.com' 7aa964b
git shortlog -sne 7aa964b
git rev-list --count 7aa964b..00e6ec6
git log --reverse --format='%h %ad %s' --date=short 7aa964b..00e6ec6
git diff --stat 7aa964b 00e6ec6 -- pintos/
git show 767f006 -- pintos/vm/vm.c
git show b38cc90 -- pintos/threads/thread.c
git show 15dacef -- pintos/tests/threads/priority-donate-ready.c
git show 4d1cd47 -- pintos/threads/thread.c pintos/threads/synch.c
```

기존 커밋 메시지·저자·해시는 보존한다. 과거 제목의 언어·형식을 일괄 바꾸면 팀 이력과 문서의 커밋 링크도 달라진다. 새로운 정리는 이슈와 PR로 남기고, 현재 설명의 오류는 문서에서 바로잡는다.
