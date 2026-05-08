# Pintos BAD 테스트와 유저 포인터 검증 흐름 정리

## 질문의 핵심

내가 한 질문은 다음 내용이었다.

> 나는 `bad-read`처럼 유저 코드가 직접 잘못된 주소를 만지는 문제를 고치고 있었는데, 왜 갑자기 `check_user_string(file)` 같은 syscall 문자열 검증 피드백이 나왔을까?

결론부터 말하면, 둘은 직접 같은 버그는 아니지만 같은 BAD 계열 안에서 연결된다.

BAD 계열 테스트는 크게 두 흐름으로 나뉜다.

```text
1. 유저 코드가 직접 잘못된 주소를 접근
   예: *(int *) NULL, *(int *) 0x8004000000
   흐름: user code -> page fault -> exception.c

2. syscall 인자로 잘못된 포인터를 전달
   예: open((char *) bad_addr), create((char *) bad_addr)
   흐름: user code -> syscall -> syscall.c -> 커널이 포인터 사용
```

따라서 `bad-read` 하나만 보면 `exception.c`의 page fault 처리가 핵심이고, BAD 계열 전체를 보면 `syscall.c`의 유저 포인터 검증도 같이 봐야 한다.

## 1. bad-read 계열은 왜 exception.c 문제인가

`bad-read`, `bad-write`, `bad-read2`, `bad-write2`는 유저 프로그램이 직접 잘못된 주소를 읽거나 쓴다.

예를 들면 다음과 같다.

```c
*(int *) NULL;
*(int *) NULL = 42;
*(int *) 0x8004000000;
*(int *) 0x8004000000 = 42;
```

이 코드는 syscall을 거치지 않는다. 유저 코드가 실행되는 중 CPU가 잘못된 메모리 접근을 감지하고 page fault를 발생시킨다.

흐름은 다음과 같다.

```text
유저 프로그램 실행
-> 잘못된 주소 직접 접근
-> CPU page fault 발생
-> exception.c의 page_fault() 진입
-> 유저 코드에서 난 fault라면 exit(-1)
```

이때 `intr_frame`의 `cs` 값이 중요하다.

```text
f->cs == SEL_UCSEG
```

이면 유저 코드 실행 중 발생한 예외라는 뜻이다. 이 경우 커널 panic으로 처리하면 안 되고, 잘못한 유저 프로세스만 `exit(-1)`로 종료해야 한다.

그래서 `page_fault()` 쪽에서 다음 방향이 필요하다.

```c
#ifdef VM
	if (vm_try_handle_fault (f, fault_addr, user, write, not_present))
		return;
#endif

page_fault_cnt++;

if (f->cs == SEL_UCSEG)
	process_exit_with_status (-1);
```

여기서 `page_fault_cnt++`를 먼저 두는 이유는 유저 page fault도 통계에 잡히게 하기 위해서다.

## 2. check_user_string(file)은 왜 syscall.c 문제인가

`open-bad-ptr`, `create-bad-ptr`, `exec-bad-ptr` 같은 테스트는 유저 코드가 직접 bad address를 읽지 않는다.

대신 잘못된 포인터를 syscall 인자로 넘긴다.

```c
open ((char *) bad_address);
create ((char *) bad_address, 0);
exec ((char *) bad_address);
```

이 경우 흐름은 다음과 같다.

```text
유저 프로그램이 syscall 호출
-> 커널 syscall_handler() 진입
-> syscall.c의 open/create/exec 실행
-> 커널이 유저가 넘긴 문자열 포인터를 읽음
```

문제는 커널이 `filesys_open(file)`이나 `filesys_create(file)` 안에서 문자열을 읽게 된다는 점이다.

만약 syscall 진입 시점에 문자열 전체를 검증하지 않으면, 커널이 유저 포인터를 따라가다가 page fault를 낼 수 있다.

## 3. check_address(file)만으로 부족한 이유

`check_address(file)`은 포인터가 가리키는 첫 주소 하나만 검사한다.

하지만 파일명은 문자열이다.

```text
"sample.txt\0"
```

문자열은 `\0`을 만날 때까지 계속 읽어야 끝난다.

따라서 첫 주소만 유효하다고 문자열 전체가 안전한 것은 아니다.

위험한 상황은 다음과 같다.

```text
file 포인터 시작 주소: 유효한 유저 페이지
문자열 중간 또는 끝: 다음 페이지로 넘어감
다음 페이지: 미매핑
```

이 경우 `check_address(file)`은 통과한다.

하지만 이후 커널이 문자열을 읽다가 다음 페이지에 접근하면 page fault가 발생할 수 있다.

더 위험한 점은 이 page fault가 유저 코드가 아니라 syscall 처리 중인 커널 코드에서 날 수 있다는 것이다.

```text
bad-read:
  유저 코드에서 page fault
  f->cs == SEL_UCSEG
  exception.c에서 exit(-1) 처리 가능

open-bad-ptr:
  syscall 처리 중 커널이 유저 문자열을 읽다가 page fault
  f->cs == SEL_KCSEG일 수 있음
  커널 버그 또는 panic으로 이어질 수 있음
```

그래서 syscall 인자는 커널이 사용하기 전에 미리 검증해야 한다.

## 4. check_user_string(file)이 필요한 이유

`check_user_string(file)`의 목적은 문자열 시작 주소 하나만 보는 것이 아니라, `\0`을 만날 때까지 모든 주소가 유효한지 확인하는 것이다.

개념적으로 역할은 다음과 같다.

```text
check_address(file)
= file[0] 위치만 안전한지 검사

check_user_string(file)
= file[0]부터 '\0'까지 문자열 전체가 안전한지 검사
```

따라서 문자열 인자를 받는 syscall은 `check_address()`보다 `check_user_string()`이 더 적절하다.

대상 예시는 다음과 같다.

```text
fork(thread_name)
create(file)
open(file)
remove(file)
exec(cmd_line)
```

단, `exec()`는 이미 `check_user_string(cmd_line)`을 쓰고 있다면 방향이 맞다.

## 5. 방금 수정한 코드와의 관계

방금 수정한 `page_fault()` 관련 코드는 다음 테스트 흐름에 직접 연결된다.

```text
bad-read
bad-write
bad-read2
bad-write2
```

이 테스트들은 유저 코드가 직접 잘못된 주소를 건드리는 흐름이다.

반면 `check_user_string(file)`은 다음 테스트 흐름과 연결된다.

```text
create-bad-ptr
open-bad-ptr
exec-bad-ptr
open-boundary
create-bound
exec-boundary
```

즉 둘의 관계는 다음처럼 정리할 수 있다.

```text
bad-read 하나만 고치기:
  exception.c의 page_fault 처리만 보면 됨

BAD 계열 전체를 안정적으로 고치기:
  exception.c page_fault 처리
  syscall.c 유저 포인터 검증
  둘 다 필요함
```

## 6. 왜 이 피드백이 나온 것인가

`check_user_string(file)` 피드백은 `bad-read` 하나만 놓고 보면 직접적인 수정은 아니다.

하지만 BAD 계열을 넓게 보면 관련이 있다.

공통 목표는 다음과 같다.

```text
잘못된 유저 주소가 들어와도 커널이 죽으면 안 된다.
잘못한 유저 프로세스만 exit(-1)로 종료되어야 한다.
```

이 목표를 달성하려면 두 방어선이 필요하다.

```text
1. 유저가 직접 잘못된 주소를 접근한 경우
   -> exception.c에서 page fault를 exit(-1)로 처리

2. 유저가 syscall 인자로 잘못된 포인터를 넘긴 경우
   -> syscall.c에서 커널이 포인터를 사용하기 전에 검증
```

따라서 `check_user_string(file)`은 갑자기 나온 별개의 피드백이 아니라, BAD 계열 전체를 안정적으로 통과시키기 위한 다음 단계 피드백이다.

## 7. 핵심 정리

핵심은 다음 한 문장이다.

```text
bad-read와 check_user_string(file)은 직접 같은 버그는 아니지만,
둘 다 잘못된 유저 주소가 커널을 죽이지 않고 해당 프로세스만 exit(-1)로 끝나게 하는 문제와 관련되어 있다.
```

구분해서 기억하면 된다.

```text
유저 코드가 직접 잘못된 주소 접근
-> exception.c / page_fault()

syscall 인자로 잘못된 포인터 전달
-> syscall.c / check_address(), check_user_string(), check_user_buffer()
```

문자열은 첫 주소가 아니라 `\0`까지 검사해야 하고, 버퍼는 첫 주소가 아니라 `size`만큼 걸친 모든 페이지를 검사해야 한다.
