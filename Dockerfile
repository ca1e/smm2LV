FROM ghcr.m.daocloud.io/webassembly/wasi-sdk:wasi-sdk-34 AS builder

WORKDIR /src
ADD . /src/

RUN make PLATFORM=web BUILD=release && rm -rf bin/src

# binaryen (from the emsdk image) shrinks the module: ~4MB -> ~650KB,
# then node's zlib emits the pre-compressed brotli variant (~150KB)
FROM emscripten/emsdk:6.0.9 AS optimizer

COPY --from=builder /src/bin/toost.wasm /in/toost.wasm
COPY --from=builder /src/index.html /in/index.html
COPY --from=builder /src/img /in/img/

RUN /emsdk/upstream/bin/wasm-opt -Oz --strip-debug /in/toost.wasm -o /in/toost-opt.wasm \
	&& node -e "const z=require('zlib'),f=require('fs'),w=f.readFileSync('/in/toost-opt.wasm');f.writeFileSync('/in/toost-opt.wasm.br',z.brotliCompressSync(w,{params:{[z.constants.BROTLI_PARAM_QUALITY]:11}}));console.log('brotli:',(w.length/1024).toFixed(0)+'KB ->',(f.statSync('/in/toost-opt.wasm.br').size/1024).toFixed(0)+'KB');" \
	&& mkdir /out \
	&& cp /in/toost-opt.wasm /out/toost.wasm \
	&& cp /in/toost-opt.wasm.br /out/toost.wasm.br \
	&& cp /in/index.html /out/ \
	&& cp -r /in/img /out/img

FROM alpine:3.19

# 国内网络：apk 换清华镜像源
RUN sed -i 's#https://dl-cdn.alpinelinux.org#https://mirrors.tuna.tsinghua.edu.cn#' /etc/apk/repositories \
	&& apk add --no-cache busybox-extras

COPY --from=optimizer /out/ /app/

USER nobody
EXPOSE 8080
HEALTHCHECK --interval=30s --timeout=3s CMD wget -qO- http://127.0.0.1:8080/ >/dev/null 2>&1 || exit 1
CMD ["httpd", "-f", "-p", "8080", "-h", "/app"]
