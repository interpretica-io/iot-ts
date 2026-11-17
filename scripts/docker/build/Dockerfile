# Build environment
FROM debian:bookworm

ARG user
ARG uid
ARG gid
ARG docker_gid
ENV USER=${user}
ENV UID=${uid}
ENV GID=${gid}
ENV DOCKER_GID=${docker_gid}

RUN if [ "$DOCKER_GID" != "${GID}" ] ; then groupadd --gid ${DOCKER_GID} docker ; fi

RUN if [ ! -d /home/${USER} ] ; then mkdir -p /home/${USER} && chmod -R 777 /home/${USER} ; fi

# Install basic dependencies & useful tools. libcoap v3 is NOT in the
# distro here, so it is built from source below; ta_coap links -lcoap-3.
RUN apt-get update && \
    apt-get install -y \
        autoconf \
        libtool \
        automake \
        bison \
        build-essential \
        ccache \
        cmake \
        curl \
        docker.io \
        file \
        flex \
        gawk \
        libbsd-dev \
        libavahi-client-dev \
        libavahi-common-dev \
        libmodbus-dev \
        libsensors-dev \
        autoconf-archive \
        libjansson-dev \
        libexpat1 \
        libexpat1-dev \
        libcurl4-openssl-dev \
        libglib2.0-dev \
        libnetconf2-dev \
        libncurses5 \
        libncurses5-dev \
        libpam0g \
        libpam0g-dev \
        libpcap0.8-dev \
        libpopt-dev \
        libreadline-dev \
        libssh-dev \
        libssh2-1-dev \
        libssl-dev \
        libxml2 \
        libxml2-dev \
        libxslt1-dev \
        libxslt1.1 \
        libpcre3-dev \
        libyaml-dev \
        m4 \
        meson \
        openssh-server \
        pkg-config \
        perl \
        python3 \
        rsync \
        ssh \
        sudo \
        tshark \
        tcl-dev \
        unzip \
        wget

# libcoap v3 from source (no distro package): ta_coap needs coap3/coap.h
# and -lcoap-3. Pinned to a release tag; adjust on a real build if the
# tag or configure flags drift.
RUN set -eux; cd /tmp; \
    git clone --depth 1 --branch v4.3.5 https://github.com/obgm/libcoap.git; \
    cd libcoap; ./autogen.sh; \
    ./configure --disable-doxygen --disable-manpages --disable-tests \
                --disable-examples --with-openssl; \
    make -j"$(nproc)"; make install; ldconfig; \
    cd /; rm -rf /tmp/libcoap

# libgpiod v2 from source: Debian bookworm ships libgpiod v1.6, but
# ta_gpio uses the v2 API (gpiod_line_value, GPIOD_LINE_VALUE_*). Build
# v2 from source and install it to /usr so pkg-config finds libgpiod 2.x.
RUN set -eux; cd /tmp; \
    git clone --depth 1 --branch v2.3.2 https://github.com/brgl/libgpiod.git; \
    cd libgpiod; \
    meson setup build --prefix=/usr -Dtools=disabled -Dtests=disabled; \
    ninja -C build; ninja -C build install; ldconfig; \
    cd /; rm -rf /tmp/libgpiod

# lm-sensors (libsensors-dev) ships no pkg-config file, but the
# ta_rpcprovider app resolves its --extra-deps only through pkg-config.
# Provide a minimal libsensors.pc so `dependency('libsensors')` finds it
# and ta_sensors links -lsensors.
RUN set -eux; \
    printf '%s\n' \
      'prefix=/usr' \
      'libdir=${prefix}/lib/x86_64-linux-gnu' \
      'includedir=${prefix}/include' \
      'Name: libsensors' \
      'Description: lm-sensors library' \
      'Version: 3.6.0' \
      'Libs: -L${libdir} -lsensors' \
      'Cflags: -I${includedir}' \
      > /usr/lib/x86_64-linux-gnu/pkgconfig/libsensors.pc

RUN echo "${USER} ALL=NOPASSWD: ALL" >> /etc/sudoers

RUN if [ "$DOCKER_GID" != "${GID}" ] ; then groupadd --gid ${GID} ${USER} ; fi

RUN useradd --system \
            --gid ${GID} --gid ${DOCKER_GID} \
            --uid ${UID} \
            --shell /bin/bash \
            ${USER}
RUN usermod -a -G ${DOCKER_GID} ${USER}
RUN usermod -a -G docker ${USER}
