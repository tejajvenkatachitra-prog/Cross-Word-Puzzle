# ---- Build stage ----
FROM debian:bookworm-slim AS build
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential ca-certificates \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY . .
RUN g++ -std=c++17 -O2 -pthread main.cpp -o server

# ---- Runtime stage ----
FROM debian:bookworm-slim
RUN apt-get update && apt-get install -y --no-install-recommends ca-certificates \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY --from=build /app/server ./server
RUN mkdir -p ./public
COPY index.html style.css script.js ./public/

# Render provides $PORT at runtime; the server reads it directly.
EXPOSE 8080
CMD ["./server"]
