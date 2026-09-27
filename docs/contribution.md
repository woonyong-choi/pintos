# 담당 범위와 개인 확장 커밋

[README](../README.md)의 "본인 담당 범위" 요약을 뒷받침하는 표와 커밋 목록이다.

## 팀 과제

| 항목 | 내용 |
|---|---|
| 원본 팀 저장소 | [Jungle-12-303/wk11_7](https://github.com/Jungle-12-303/wk11_7) |
| 팀 과제 기간 | 2026-04-23 ~ 2026-05-21 (커밋 `581a1e4` ~ `7aa964b`) |
| 팀 구성 | 7인. `main`에 병합된 기여자는 6인이고, 나머지 1인의 커밋은 병합되지 않은 브랜치에 남아 있다 — `git shortlog -sne --all`로 확인 |
| 본인 담당(팀 기간) | 팀 기간 `main` 커밋 159/214개. threads(alarm·priority·donation)와 userprog·vm 전반 |
| 팀원 담당 | 파일 시스템 콜, 프레임 테이블, 교체 알고리즘, mmap 본체 |

기반 커널·테스트 하니스는 KAIST 과제 제공물이다. 팀 코드 전체를 개인 구현으로 주장하지 않으며, 팀 구현과 개인 변경은 커밋 저자와 파일별 diff로 구분한다.

## 개인 확장 (팀 과제 종료 후)

커밋 범위: [`a1ff596`](https://github.com/woonyong-choi/lrn-pintos/commit/a1ff596)(2026-08-02) ~ [`e80c36b`](https://github.com/woonyong-choi/lrn-pintos/commit/e80c36b)(2026-09-23), 총 22개 커밋. `git log --author='woonyong' --since=2026-06-01 --reverse --format='%h %ad %s' --date=short` 기준.

| 무엇이 달라졌나 | 커밋 | 파일 |
|---|---|---|
| **MLFQS 구현**(팀 종료 시점에는 TODO 스텁) — 4BSD 규칙, 17.14 고정소수점, 전체 스레드 갱신용 `all_elem` 목록, MLFQS일 때 donation 우회 | [`3162890`](https://github.com/woonyong-choi/lrn-pintos/commit/3162890) | `thread.c`(+188), `thread.h`(+3), `synch.c`(+9) |
| **선점 판단 버그 수정** — `thread_create`에서 요청값 대신 실제 `t->priority` 비교. vm 회귀 재검증 | [`b38cc90`](https://github.com/woonyong-choi/lrn-pintos/commit/b38cc90) | `thread.c` |
| **x86 QEMU CI 도입** — threads·vm job 분리, `ubuntu-22.04` + `qemu-system-x86` | [`7db77ff`](https://github.com/woonyong-choi/lrn-pintos/commit/7db77ff) | `.github/workflows/ci.yml` |
| 저장소 정리 — 중복 문서 제거, 커널 범위 명확화, 출처·기여 경계 명시, README 재구성 | [`a1ff596`](https://github.com/woonyong-choi/lrn-pintos/commit/a1ff596), [`81d907b`](https://github.com/woonyong-choi/lrn-pintos/commit/81d907b), [`7286e4c`](https://github.com/woonyong-choi/lrn-pintos/commit/7286e4c), [`b56d617`](https://github.com/woonyong-choi/lrn-pintos/commit/b56d617), [`931fc42`](https://github.com/woonyong-choi/lrn-pintos/commit/931fc42), [`3ba22db`](https://github.com/woonyong-choi/lrn-pintos/commit/3ba22db) | `docs/`, `README.md` |
| 검증된 기여 수치 정정 | [`31e6d75`](https://github.com/woonyong-choi/lrn-pintos/commit/31e6d75) | `README.md` |
| **ready 상태 lock holder 기부 버그 수정** — 재현 테스트를 먼저 쓰고 고쳤다. 불변식 소유자를 `thread_set_effective_priority()` 하나로 통일 | [`15dacef`](https://github.com/woonyong-choi/lrn-pintos/commit/15dacef), [`4d1cd47`](https://github.com/woonyong-choi/lrn-pintos/commit/4d1cd47) | `priority-donate-ready.c`(신규), `thread.c`, `synch.c` |
| **설계 근거 문서화** — 고른 자료구조·버린 대안·알려진 한계 | [`2ec378a`](https://github.com/woonyong-choi/lrn-pintos/commit/2ec378a) | `docs/design.md`(신규) |
| 죽은 코드·유령 파일 제거 — 참조 없는 스크립트·이미지·문서, 구현이 끝난 자리에 남은 TODO | [`e820dc4`](https://github.com/woonyong-choi/lrn-pintos/commit/e820dc4) | `vm.c`, `synch.c`, `docs/`, `scripts/` |
| 데모 GIF 교체, 구조도 추가, 커밋 링크 재작성 이력 반영, 통과 수 재실측 | [`3624759`](https://github.com/woonyong-choi/lrn-pintos/commit/3624759), [`25115a7`](https://github.com/woonyong-choi/lrn-pintos/commit/25115a7), [`7bc09f8`](https://github.com/woonyong-choi/lrn-pintos/commit/7bc09f8), [`426c648`](https://github.com/woonyong-choi/lrn-pintos/commit/426c648) | `docs/demo.gif`, `docs/figure.png`, `README.md` |
| **userprog CI job 추가** — `.github/workflows/ci.yml`에 `user-programs` job 분리, 검사 표 문서화 | [`e80c36b`](https://github.com/woonyong-choi/lrn-pintos/commit/e80c36b) | `.github/workflows/ci.yml`, `README.md` |

팀 과제 종료 시점(`7aa964b`) 대비 커널 코드 순증은 `pintos/` 기준 `git diff --stat 7aa964b HEAD -- pintos/`로 확인한다.
