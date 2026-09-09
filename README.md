# lrn-pintos

스레드가 CPU를 얻는 순서와 프로세스가 메모리를 공유하고 회수하는 과정을 구현한 교육용 커널이다. KAIST PintOS의 x86-64 코드 위에 우선순위 스케줄링, 시스템 콜, 가상 메모리와 Copy-on-Write fork를 구현했다.

크래프톤 정글 12기 팀 프로젝트 [Jungle-12-303/wk11_7](https://github.com/Jungle-12-303/wk11_7)을 보존하고 이어서 작업하는 개인 미러다. 기반 커널과 테스트는 과제 제공물이며, 팀 구현과 개인 변경은 커밋 저자와 파일별 변경 이력으로 구분한다.

## 실행

x86-64 Linux, GCC·Make·Perl과 QEMU가 필요하다. macOS arm64에서는 커널을 직접 빌드하지 않고 저장소의 x86-64 Dev Container를 사용한다. 컨테이너의 Bash 터미널에서 저장소 루트로 이동한 뒤 실행한다.

```bash
source pintos/activate
make -C pintos/threads check
make -C pintos/vm check
```

`activate`는 이 저장소의 `pintos/utils`를 PATH에 추가한다. Dev Container의 예전 경로 설정에 의존하지 않도록 직접 실행한다. 두 명령은 커널을 빌드한 뒤 QEMU에서 스레드와 가상 메모리 테스트를 각각 실행한다. 개별 실행 출력과 판정은 각 디렉터리의 `build/tests/`에 남는다.

[2026-09-08 CI](https://github.com/woonyong-kr/lrn-pintos/actions/runs/34203370471)에서 이 README 변경 전의 코드로 Ubuntu 22.04 x86-64의 두 검증이 통과했다. 현재 실행 상태는 [Actions](https://github.com/woonyong-kr/lrn-pintos/actions)에서 확인할 수 있다.

## 구현에서 볼 부분

**우선순위와 락 대기.** 준비된 스레드 중 우선순위가 높은 스레드를 선택하고, 락을 기다리는 스레드의 우선순위를 락 소유자에게 전달한다. 우선순위 변경이 실행 순서와 대기 목록에 어떤 영향을 주는지는 [thread.c](pintos/threads/thread.c)와 [synch.c](pintos/threads/synch.c)에서 확인할 수 있다.

**쓰는 순간에 복사하는 fork.** 부모와 자식이 같은 프레임을 읽기 전용으로 공유하다가 쓰기 페이지 폴트가 나면 복사한다. 참조가 하나만 남으면 복사 없이 쓰기 권한을 되돌린다. [vm.c의 `vm_handle_cow`](pintos/vm/vm.c)가 이 두 경로를 나눈다.

**공유 상태와 회수 조건.** [프레임](pintos/include/vm/vm.h)은 참조 수와 소유 스레드를 함께 가진다. 스왑으로 내보낼 때는 소유자의 페이지 테이블 항목도 정리해야 하기 때문이다. 현재는 참조 수가 1인 프레임만 교체 대상으로 고른다. 여러 프로세스가 공유 중인 프레임까지 자유롭게 스왑하는 구현은 아니다.

그 밖에 demand paging, mmap, stack growth와 [익명 페이지 swap](pintos/vm/anon.c)을 다룬다. [process.c](pintos/userprog/process.c)는 fork·exec·wait와 부모·자식 종료를 처리하며, 부모가 먼저 종료한 뒤 남는 자식 상태의 회수도 포함한다.

## 범위와 한계

이 저장소의 대상은 교육용 커널이다. 범용 OS의 장치 지원, 보안, 다중 코어 확장이나 운영 서비스의 안정성을 검증한 결과로 보지는 않는다. QEMU 테스트 통과도 모든 메모리 압박·스케줄링 조합에서의 정확성을 보장하지 않는다.

팀 저장소와 이 미러는 서로 다른 시점의 코드를 가질 수 있다. 변경을 비교할 때는 저장소 이름뿐 아니라 커밋을 함께 확인한다. 특히 COW와 swap은 프레임의 공유 상태와 해제 책임이 연결돼 있으므로 함수 하나만 떼어 비교하기 어렵다.

## 관련 자료

- [KAIST PintOS 과제 안내](https://casys-kaist.github.io/pintos-kaist/)
- [단계별 구현 기록](docs/pintos/)
- [부모 종료 후 자식 상태 회수](docs/pintos/10-orphan-reaping.md)
- [원본 커널 라이선스](pintos/LICENSE)
