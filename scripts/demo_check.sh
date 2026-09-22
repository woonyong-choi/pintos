#!/bin/bash
# 터미널 데모: threads 테스트 일부를 실제 QEMU에서 make check 로 실행한다. 사전 조건: docker
#   이미지 1회 빌드: docker build --load -t pintos-demo -f scripts/Dockerfile.demo scripts
# 저장소를 임시 폴더로 복사해서 실행하므로 작업 트리는 바뀌지 않는다. 처음 한 번은 커널 빌드가 추가된다.
cd "$(dirname "$0")/.." || exit 1
echo "# lrn-pintos: QEMU에서 make check 실행 (threads 테스트 6개, 2배속 재생)"
WORK=${WORK:-/tmp/pintos-run}
[ -d "$WORK/pintos" ] || { mkdir -p "$WORK" && cp -R . "$WORK"; }
TESTS="alarm-single alarm-priority priority-change priority-donate-one priority-donate-nest priority-sema"
LIST=$(for t in $TESTS; do printf 'tests/threads/%s ' "$t"; done)
echo "# (준비) 커널 빌드 확인·이전 결과 삭제 ..."
docker run --rm -v "$WORK":/w pintos-demo bash -c 'source pintos/activate >/dev/null 2>&1; make -C pintos/threads >/dev/null 2>&1; cd pintos/threads/build && rm -f tests/threads/*.result tests/threads/*.output tests/threads/*.errors' 2>/dev/null
printf '\033[1;32m$\033[0m make -C pintos/threads check TESTS="%s" | grep -E "^(pass|FAIL|All)"\n' "${LIST% }"
docker run --rm -v "$WORK":/w pintos-demo bash -c "source pintos/activate >/dev/null 2>&1; make -C pintos/threads check TESTS='$LIST' 2>&1 | grep --line-buffered -E '^(pass|FAIL|All)'" 2>/dev/null
sleep 2.5
