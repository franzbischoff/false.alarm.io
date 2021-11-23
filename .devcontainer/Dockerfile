FROM python:3.9.0-slim

ARG CORE_VERSION="5.2.3"
ARG CORE_DESCR="platformio-core"
ARG BUILD_NAME="false.alarm.io dev"
ARG BUILD_DESCRIPTION="ready to use environment for false.alarm.io using Platformio"
ARG BUILD_VERSION=1
ARG BUILD_DATE="2021-11-10"
ARG BUILD_REPOSITORY="franzbischoff/false.alarm.io"
ARG HOME="/home/docker"

ARG DEBIAN_FRONTEND noninteractive

RUN apt-get update -qq \
  && apt-get dist-upgrade -y \
  && rm -rf /var/lib/apt/lists/*

# RUN add-apt-repository ppa:apt-fast/stable && apt-get update && apt-get install -y apt-fast

# Install base packages
RUN apt-get update && apt-get install -y --no-install-recommends \
  software-properties-common\
  build-essential \
  sudo \
  vim \
  git \
  curl \
  libgit2-dev \
  libcurl4-openssl-dev \
  libssl-dev \
  libxml2-dev \
  libzmq3-dev\
  libpython2.7 \
  libncurses5 \
  libncurses5-dev \
  flex \
  bison \
  cmake \
  gnupg \
  gpg-agent \
  expect \
  wget \
  gperf \
  locales \
  python3 \
  python3-pip \
  python3-setuptools \
  ninja-build \
  ccache \
  libffi-dev \
  libssl-dev \
  dfu-util \
  libusb-1.0-0 \
  zsh \
  zsh-autosuggestions \
  zsh-syntax-highlighting \
  mlocate \
  && rm -rf /var/lib/apt/lists/*

RUN useradd -p '' docker && \
  usermod --shell /bin/zsh docker && \
  usermod -aG sudo,dialout,plugdev docker

COPY .zshrc ${HOME}/.zshrc

RUN git clone --depth 1 git://github.com/ohmyzsh/ohmyzsh ${HOME}/.oh-my-zsh \
  && chown -R docker:docker ${HOME}

RUN /usr/local/bin/python -m pip install --upgrade pip

USER docker
WORKDIR $HOME

RUN pip install -U --no-cache-dir MarkupSafe==2.0.1 \
  && mkdir -p ${HOME}/.platformio \
  && chmod a+rwx ${HOME}/.platformio

USER root
WORKDIR /tmp

RUN wget -nv https://raw.githubusercontent.com/platformio/platformio-core/master/scripts/99-platformio-udev.rules \
  && mv /tmp/99-platformio-udev.rules /lib/udev/rules.d/99-platformio-udev.rules

# Fix for pio run -t menuconfig
RUN localedef -c -i en_US -f UTF-8 en_US.UTF-8

RUN chown -R docker:docker ${HOME}

# cleanunp
RUN apt-get clean autoclean && apt-get autoremove --yes \
  && rm -rf /var/lib/apt/lists/* \
  && rm -rf /var/lib/cache/* \
  && rm -rf /var/lib/log/*

RUN updatedb

# Labels
LABEL \
  org.opencontainers.image.title=${BUILD_NAME} \
  org.opencontainers.image.description=${BUILD_DESCRIPTION} \
  org.opencontainers.image.authors="Francisco Bischoff <fbischoff@med.up.pt>" \
  org.opencontainers.image.licenses="CC-BY-NC-SA/4.0" \
  org.opencontainers.image.source="https://github.com/${BUILD_REPOSITORY}" \
  org.opencontainers.image.documentation="https://github.com/${BUILD_REPOSITORY}/blob/main/README.md" \
  org.opencontainers.image.created=${BUILD_DATE} \
  org.opencontainers.image.version=${APP_VERSION} \
  org.opencontainers.image.core.version=${CORE_VERSION} \
  org.opencontainers.image.core.description=${CORE_DESCR}

ENV WORKSPACE_DIRECTORY "/workspace"
ENV NOT_CI "true"

USER docker
WORKDIR /workspace

ENTRYPOINT ["platformio"]

