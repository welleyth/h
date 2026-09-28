#!/bin/sh
set -eu

if [ $# -ne 2 ]; then
    echo "usage: install-eo-judge.sh <version> <directory>" >&2
    exit 2
fi
version=$1
into=$2

case "$(uname -s)" in
    Linux) os=linux ;;
    Darwin) os=darwin ;;
    *) echo "install-eo-judge: eo-judge is released for Linux and macOS, not $(uname -s)" >&2; exit 1 ;;
esac
case "$(uname -m)" in
    x86_64 | amd64) arch=amd64 ;;
    arm64 | aarch64) arch=arm64 ;;
    *) echo "install-eo-judge: eo-judge is released for amd64 and arm64, not $(uname -m)" >&2; exit 1 ;;
esac

name="eo-judge-$os-$arch"
release="https://github.com/eolymp/h/releases/download/judge/v$version"
mkdir -p "$into"
cd "$into"
if ! curl -fsSL --retry 3 -o "$name" "$release/$name" || ! curl -fsSL --retry 3 -o SHA256SUMS "$release/SHA256SUMS"; then
    echo "install-eo-judge: cannot download $name for $version from $release; is judge/v$version released?" >&2
    exit 1
fi
if ! grep "  $name\$" SHA256SUMS > "$name.sha256"; then
    echo "install-eo-judge: SHA256SUMS of $version lists no $name" >&2
    exit 1
fi
if command -v sha256sum > /dev/null; then
    verify="sha256sum -c"
else
    verify="shasum -a 256 -c"
fi
if ! $verify "$name.sha256" > /dev/null; then
    echo "install-eo-judge: $name does not match SHA256SUMS of $version; the download is damaged or not the release" >&2
    exit 1
fi
chmod 755 "$name"
mv "$name" eo-judge
rm -f SHA256SUMS "$name.sha256"
said=$(./eo-judge version)
if [ "$said" != "eo-judge $version" ]; then
    echo "install-eo-judge: the binary of $version says \"$said\"" >&2
    exit 1
fi
echo "$said"
