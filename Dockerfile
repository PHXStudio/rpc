################################################################
# Stage 1: common dependencies
################################################################
FROM ubuntu:22.04 AS base
RUN apt-get update \
 && apt-get install -y --no-install-recommends \
        cmake g++ make bison flex git ca-certificates \
 && update-ca-certificates \
 && rm -rf /var/lib/apt/lists/*

################################################################
# Stage 2: Linux (native)
################################################################
FROM base AS linux-builder
ARG BUILD_TESTING=OFF
COPY . /src
RUN cmake -S /src -B /src/build \
        -DBUILD_TESTING=${BUILD_TESTING} \
        -DCMAKE_BUILD_TYPE=Release \
 && cmake --build /src/build

################################################################
# Stage 3: Windows (MinGW cross-compile)
################################################################
FROM base AS windows-builder
ARG BUILD_TESTING=OFF
RUN apt-get update \
 && apt-get install -y --no-install-recommends mingw-w64 \
 && rm -rf /var/lib/apt/lists/*
COPY . /src
RUN cmake -S /src -B /src/build \
        -DBUILD_TESTING=${BUILD_TESTING} \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_SYSTEM_NAME=Windows \
        -DCMAKE_C_COMPILER=x86_64-w64-mingw32-gcc \
        -DCMAKE_CXX_COMPILER=x86_64-w64-mingw32-g++ \
 && cmake --build /src/build

################################################################
# Stage 4: Linux test (gtest + cross-lang with .NET)
################################################################
FROM base AS linux-tester
RUN apt-get update \
 && apt-get install -y --no-install-recommends \
        wget apt-transport-https \
 && wget -q https://dot.net/v1/dotnet-install.sh -O /tmp/dotnet-install.sh \
 && chmod +x /tmp/dotnet-install.sh \
 && /tmp/dotnet-install.sh --channel 6.0 --install-dir /usr/share/dotnet \
 && rm -f /tmp/dotnet-install.sh \
 && rm -rf /var/lib/apt/lists/*
ENV DOTNET_ROOT=/usr/share/dotnet
ENV PATH="$PATH:/usr/share/dotnet"
COPY . /src
RUN cmake -S /src -B /src/build \
        -DBUILD_TESTING=ON \
        -DCMAKE_BUILD_TYPE=Release \
 && cmake --build /src/build --clean-first
CMD ["cmake", "--build", "/src/build", "--target", "test"]

################################################################
# Stage 5: collect artifacts
################################################################
FROM scratch AS output
COPY --from=linux-builder   /src/build/compiler/rpc     /rpc-linux
COPY --from=windows-builder /src/build/compiler/rpc.exe /rpc-windows.exe

################################################################
# Stage 6: extract (uses a base with shell for docker cp)
################################################################
FROM ubuntu:22.04 AS extract
COPY --from=output /rpc-linux /out/rpc-linux
COPY --from=output /rpc-windows.exe /out/rpc-windows.exe
