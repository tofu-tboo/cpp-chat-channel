FROM debian:bookworm-slim

RUN apt-get update && \
    apt-get install -y --no-install-recommends \
        build-essential \
        pkg-config \
        libjansson-dev && \
    rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY . .

RUN make clean || true
RUN make server client

EXPOSE 5000
CMD ["./server"]
