# Multi-stage build: compile with a full toolchain, ship only the
# resulting binary. See docs/DECISIONS.md ("Dockerize the VM") for the
# reasoning -- WhiteFang has no separate on-disk bytecode format, so
# there's nothing to containerize but the whole lex/parse/compile/
# execute pipeline in one binary.

FROM alpine:3.20 AS build
RUN apk add --no-cache gcc musl-dev make
WORKDIR /src
COPY . .
RUN make whitefang

FROM alpine:3.20
COPY --from=build /src/build/whitefang /usr/local/bin/whitefang
WORKDIR /work
ENTRYPOINT ["whitefang"]
