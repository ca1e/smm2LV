# Deployment image: serve the prebuilt wasm artifacts from dist/ together
# with index.html and img/ — no compilation happens here. Regenerate dist/
# with Dockerfile.build (`make dist`) after source changes.
FROM alpine:3.19

# 国内网络：apk 换清华镜像源
RUN sed -i 's#https://dl-cdn.alpinelinux.org#https://mirrors.tuna.tsinghua.edu.cn#' /etc/apk/repositories \
	&& apk add --no-cache busybox-extras

COPY dist/ index.html /app/
COPY img/ /app/img/

USER nobody
EXPOSE 8080
HEALTHCHECK --interval=30s --timeout=3s CMD wget -qO- http://127.0.0.1:8080/ >/dev/null 2>&1 || exit 1
CMD ["httpd", "-f", "-p", "8080", "-h", "/app"]
