FROM ubuntu:18.04

ARG DEBIAN_FRONTEND=noninteractive

RUN ln -fs /usr/share/zoneinfo/Asia/Taipei /etc/localtime \
    && apt-get update \
    && apt-get install -y -q tzdata locales \
    && dpkg-reconfigure -f noninteractive tzdata \
    && locale-gen en_US.UTF-8
RUN apt-get install -y -q curl \
    && apt-get clean \
    && apt-get purge \
    && curl -o /usr/local/bin/gosu -SL "https://github.com/tianon/gosu/releases/download/1.11/gosu-$(dpkg --print-architecture)" \
    && chmod +x /usr/local/bin/gosu

RUN apt-get install -y software-properties-common && \
    apt-add-repository ppa:git-core/ppa
RUN apt-get update && apt-get install -y \
    apt-utils \
    bison \
    ca-certificates \
    ccache \
    check \
    curl \
    flex \
    git \
    gperf \
    lcov \
    libncurses-dev \
    libusb-1.0-0-dev \
    make \
    ninja-build \
    python3 \
    python3-pip \
    unzip \
    wget \
    xz-utils \
    zip \
   && apt-get autoremove -y \
   && rm -rf /var/lib/apt/lists/* \
   && update-alternatives --install /usr/bin/python python /usr/bin/python3 10

RUN python -m pip install --upgrade pip virtualenv

ENV IDF_PATH=/opt/esp/esp-idf
ENV IDF_TOOLS_PATH=/opt/esp_tools


RUN mkdir -p $HOME/.ccache && \
  touch $HOME/.ccache/ccache.conf

COPY entrypoint.sh /usr/local/bin/entrypoint.sh

RUN rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*
ENTRYPOINT ["/usr/local/bin/entrypoint.sh"]

CMD [ "/bin/bash" ]
