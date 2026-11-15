
FROM debian:13

# apt не будет задавать вопросов во время сборки образа
ENV DEBIAN_FRONTEND=noninteractive

# 1. Компилятор, CMake, Ninja, Git и пакеты Qt 6
RUN apt-get update \
 && apt-get install -y --no-install-recommends \
        build-essential cmake ninja-build git ca-certificates \
        qt6-base-dev qt6-base-private-dev libqt6sql6-psql \
 && rm -rf /var/lib/apt/lists/*

# 2. Модуль Qt HttpServer той же версии, что и Qt из пакетов
RUN QT_VER=$(dpkg-query -W -f='${Version}' qt6-base-dev | sed 's/[+~-].*//') \
 && echo "Собираю Qt HttpServer ${QT_VER}" \
 && git clone --depth 1 --branch "v${QT_VER}" \
        https://github.com/qt/qthttpserver.git /tmp/qthttpserver \
 && cmake -S /tmp/qthttpserver -B /tmp/qthttpserver-build -G Ninja \
        -DCMAKE_INSTALL_PREFIX=/usr -DQT_BUILD_EXAMPLES=OFF -DQT_BUILD_TESTS=OFF \
 && cmake --build /tmp/qthttpserver-build \
 && cmake --install /tmp/qthttpserver-build \
 && rm -rf /tmp/qthttpserver /tmp/qthttpserver-build

WORKDIR /work