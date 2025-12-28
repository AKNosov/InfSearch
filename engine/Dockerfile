FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y \
    build-essential cmake pkg-config git \
    libssl-dev libsasl2-dev \
    libmongoc-1.0-0 libmongoc-dev \
    libbson-1.0-0 libbson-dev \
    && rm -rf /var/lib/apt/lists/*

RUN git clone --depth 1 --branch r3.10.1 https://github.com/mongodb/mongo-cxx-driver.git /tmp/mongo-cxx \
    && cmake -S /tmp/mongo-cxx -B /tmp/mongo-cxx/build \
       -DCMAKE_BUILD_TYPE=Release \
       -DBUILD_SHARED_LIBS=ON \
       -DCMAKE_INSTALL_PREFIX=/usr/local \
    && cmake --build /tmp/mongo-cxx/build -j \
    && cmake --install /tmp/mongo-cxx/build \
    && rm -rf /tmp/mongo-cxx

WORKDIR /app
COPY . /app

RUN cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
    && cmake --build build -j \
    && cp build/engine /app/engine

CMD ["./engine", "mongodb://mongo:27017", "crawler", "pages"]