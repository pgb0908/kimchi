#!/bin/bash
# kimchi 설치 스크립트 — Ubuntu 20.04
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
DIST_DIR="$SCRIPT_DIR/../dist"
BINARY="$DIST_DIR/kimchi-ubuntu20"
LIB_DIR="$DIST_DIR/lib"

if [ ! -f "$BINARY" ]; then
    echo "오류: $BINARY 파일이 없습니다. 먼저 build_ubuntu20.sh를 실행하세요."
    exit 1
fi

# 유저/그룹 생성
if ! id kimchi &>/dev/null; then
    useradd --system --no-create-home --shell /usr/sbin/nologin kimchi
fi

# 바이너리 설치
install -m 755 "$BINARY" /usr/local/bin/kimchi

# libstdc++ 번들 설치
if [ -d "$LIB_DIR" ]; then
    mkdir -p /usr/local/lib/kimchi
    cp -P "$LIB_DIR"/libstdc++.so.6* /usr/local/lib/kimchi/
    patchelf --set-rpath /usr/local/lib/kimchi /usr/local/bin/kimchi
fi

# 설정 디렉토리 초기화
mkdir -p /etc/kimchi/config
chown -R kimchi:kimchi /etc/kimchi

# 샘플 설정 복사 (없을 때만)
SAMPLES_DIR="$SCRIPT_DIR/../samples/basic"
if [ -d "$SAMPLES_DIR" ]; then
    for f in "$SAMPLES_DIR"/*.json; do
        dest="/etc/kimchi/config/$(basename "$f")"
        if [ ! -f "$dest" ]; then
            cp "$f" "$dest"
            chown kimchi:kimchi "$dest"
        fi
    done
fi

# systemd 서비스 등록
install -m 644 "$SCRIPT_DIR/kimchi.service" /etc/systemd/system/kimchi.service
systemctl daemon-reload
systemctl enable kimchi

echo ""
echo "설치 완료."
echo "  시작:  systemctl start kimchi"
echo "  상태:  systemctl status kimchi"
echo "  로그:  journalctl -u kimchi -f"
echo "  설정:  /etc/kimchi/config/"
