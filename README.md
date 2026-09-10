# 🐧 lrn-pintos

스레드의 실행 순서와 프로세스의 메모리 공유·회수를 구현한 교육용 커널입니다. KAIST PintOS x86-64 위에 우선순위 스케줄링, 시스템 콜, 가상 메모리와 Copy-on-Write fork를 구현합니다.

[PintOS Wiki](https://docs.woonyong.com/wiki/pintos/) · [단계별 구현 기록](docs/pintos/) · [CI](https://github.com/woonyong-kr/lrn-pintos/actions)

[크래프톤 정글 팀 프로젝트](https://github.com/Jungle-12-303/wk11_7)에서 이어서 작업하는 개인 저장소입니다. 기반 커널·테스트는 과제 제공물이며 팀 구현과 개인 변경은 커밋 저자·파일별 diff로 구분합니다.

## 실행과 검증

x86-64 Linux, GCC, Make, Perl과 QEMU가 필요합니다. macOS arm64에서는 저장소의 x86-64 Dev Container를 사용합니다. 컨테이너의 Bash에서 저장소 루트로 이동한 뒤 실행합니다.

```bash
source pintos/activate
make -C pintos/threads check
make -C pintos/vm check
```

`activate`는 현재 저장소의 `pintos/utils`를 PATH에 추가합니다. 각 명령은 커널을 빌드하고 QEMU에서 검사를 실행합니다. 개별 실행 출력과 판정은 해당 디렉터리의 `build/tests/`에 남습니다.

## 구현과 설계

- [thread.c](pintos/threads/thread.c)·[synch.c](pintos/threads/synch.c): 높은 우선순위부터 실행하고 lock 대기 시 소유자에게 priority를 donation합니다.
- [vm.c](pintos/vm/vm.c): fork에서 프레임을 읽기 전용으로 공유하다 쓰기 page fault에서 복사합니다. 참조가 하나라면 복사 없이 쓰기 권한을 돌려줍니다.
- [vm.h](pintos/include/vm/vm.h)·[anon.c](pintos/vm/anon.c): 프레임의 참조 수와 소유자를 관리하며 swap에서 소유자의 page table도 정리합니다. **참조 수가 1인 프레임만 교체 대상으로 선택**합니다.
- [process.c](pintos/userprog/process.c): fork·exec·wait, 부모·자식 종료와 부모가 먼저 종료한 자식 상태의 회수.

Demand paging, mmap과 stack growth도 포함합니다. COW·swap은 공유 프레임의 소유권과 해제 책임이 연결되므로 관련 경로를 함께 읽는 것이 좋습니다.

## 현재 범위와 출처

교육용 커널로서 범용 OS의 장치·보안·다중 코어 지원을 목표로 하지 않습니다. QEMU 검사 통과가 모든 메모리 압박·스케줄링 조합의 정확성을 보장하지 않으며 공유 중인 프레임까지 자유롭게 swap하는 구현은 아닙니다. 팀 원본과 비교할 때는 저장소 이름뿐 아니라 커밋을 함께 확인합니다.

[KAIST 과제 안내](https://casys-kaist.github.io/pintos-kaist/) · [부모 종료 후 회수 설계](docs/pintos/10-orphan-reaping.md) · [커널 라이선스](pintos/LICENSE)
