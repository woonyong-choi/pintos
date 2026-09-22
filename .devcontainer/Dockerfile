# Base image: Ubuntu x86_64
FROM --platform=linux/amd64 ubuntu:22.04

ENV TZ=Asia/Seoul LANG=ko_KR.UTF-8 LANGUAGE=ko_KR.UTF-8

# Install dependencies
RUN export DEBIAN_FRONTEND=noninteractive && \
    apt-get update \
    && apt-get install -y \
    locales tzdata \
    build-essential \
    gcc \
    gdb \
    vim \
    git \
    sudo \
    qemu-system-x86 \
    python3 \
    && locale-gen ko_KR.UTF-8 && update-locale LANG=ko_KR.UTF-8 \
    && rm -rf /var/lib/apt/lists/*

RUN useradd -m -s /bin/bash jungle && usermod -aG sudo jungle
RUN echo "jungle ALL=(ALL) NOPASSWD:ALL" > /etc/sudoers.d/jungle

USER jungle
WORKDIR /home/jungle

# 컨테이너 셸에서 pintos/utils를 PATH에 올린다.
# 저장소 폴더 이름이 바뀌어도 따라가도록 와일드카드로 찾는다.
RUN echo 'for a in /workspaces/*/pintos/activate; do [ -f "$a" ] && source "$a" && break; done' \
    >> /home/jungle/.bashrc
