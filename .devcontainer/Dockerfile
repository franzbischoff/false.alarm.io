FROM python:3.9.0-slim

ARG CORE_VERSION="5.2.3"
ARG CORE_DESCR="platformio-core"
ARG BUILD_NAME="false.alarm.io dev"
ARG BUILD_DESCRIPTION="ready to use environment for false.alarm.io using Platformio"
ARG BUILD_VERSION=1
ARG BUILD_DATE="2021-11-10"
ARG BUILD_REPOSITORY="franzbischoff/false.alarm.io"
ARG HOME="/home/docker"

ENV DEBIAN_FRONTEND noninteractive

RUN apt-get update -qq \
  && apt-get dist-upgrade -y

RUN add-apt-repository ppa:apt-fast/stable && apt-get update && apt-get install -y apt-fast

# Install base packages
RUN apt-fast update && apt-fast install -y --no-install-recommends \
  build-essential \
  git \
  curl \
  libgit2-dev \
  libcurl4-openssl-dev \
  libssl-dev \
  libxml2-dev \
  libzmq3-dev\
  gpg-agent \
  expect \
  zsh \
  zsh-autosuggestions \
  zsh-syntax-highlighting \
  mlocate \
  && rm -rf /var/lib/apt/lists/*

RUN useradd -ms /bin/zsh docker -p docker && \
  usermod -aG sudo docker

COPY .zshrc ${HOME}/.zshrc

RUN git clone --depth 1 git://github.com/ohmyzsh/ohmyzsh ${HOME}/.oh-my-zsh

RUN pip install -U platformio==${CORE_VERSION} && \
  mkdir -p ${HOME}/workspace && \
  mkdir -p ${HOME}/.platformio && \
  chmod a+rwx ${HOME}/.platformio

ARG PATH=${HOME}/.platformio/penv/bin:$PATH

RUN pio system completion install --shell zsh

RUN chown -R docker:docker ${HOME}

# cleanunp
RUN apt update && apt install -y git && apt-get clean autoclean && apt-get autoremove --yes && rm -rf /var/lib/{apt,dpkg,cache,log}/

RUN updatedb

# Labels
LABEL \
  maintainer="Francisco Bischoff <fbischoff@med.up.pt>" \
  org.opencontainers.image.title=${BUILD_NAME} \
  org.opencontainers.image.description=${BUILD_DESCRIPTION} \
  org.opencontainers.image.authors="Francisco Bischoff <fbischoff@med.up.pt>" \
  org.opencontainers.image.licenses="CC-BY-NC-SA/4.0" \
  org.opencontainers.image.source="https://github.com/${BUILD_REPOSITORY}" \
  org.opencontainers.image.documentation="https://github.com/${BUILD_REPOSITORY}/blob/main/README.md" \
  org.opencontainers.image.created=${BUILD_DATE} \
  org.opencontainers.image.version=${APP_VERSION} \
  org.opencontainers.image.core_version=${CORE_VERSION} \
  org.opencontainers.image.core_description=${CORE_DESCR}

ENV WORKSPACE_DIRECTORY "/workspace"
ENV NOT_CI "true"

USER docker

WORKDIR /workspace

ENTRYPOINT ["platformio"]

