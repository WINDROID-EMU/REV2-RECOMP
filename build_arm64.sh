#!/usr/bin/env bash
# ==============================================================================
# Resident Evil: Revelations 2 (REV2-RECOMP) - Compilação para Linux ARM64 (aarch64)
# ==============================================================================

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
BUILD_DIR="${SCRIPT_DIR}/build-arm64"
ARM64_SDK="${ROOT_DIR}/linux-arm64"
if [[ ! -d "${ARM64_SDK}" ]]; then
    ARM64_SDK="${SCRIPT_DIR}/android/rexglue-sdk"
fi
RUNTIME_DIR="${SCRIPT_DIR}/runtime/linux-arm64"

echo "=== Configurando compilação ARM64 (aarch64) ==="

# 1. Limpar variáveis de ambiente que possam conflitar com o cross-compiler
unset LDFLAGS CMAKE_SYSROOT CXX CC AR RANLIB LD_LIBRARY_PATH ZLIB_ROOT SDL3_DIR

# 2. Configurar o projeto com CMake e Ninja
cmake -S "${SCRIPT_DIR}" -B "${BUILD_DIR}" \
    -DCMAKE_SYSTEM_NAME=Linux \
    -DCMAKE_SYSTEM_PROCESSOR=aarch64 \
    -DCMAKE_C_COMPILER=clang-20 \
    -DCMAKE_C_COMPILER_TARGET=aarch64-linux-gnu \
    -DCMAKE_CXX_COMPILER=clang++-20 \
    -DCMAKE_CXX_COMPILER_TARGET=aarch64-linux-gnu \
    -DCMAKE_C_FLAGS="-march=armv8-a" \
    -DCMAKE_CXX_FLAGS="-march=armv8-a" \
    -DCMAKE_EXE_LINKER_FLAGS="-fuse-ld=bfd --rtlib=libgcc -Wl,--allow-shlib-undefined" \
    -DCMAKE_PREFIX_PATH="${ARM64_SDK}" \
    -DCMAKE_BUILD_TYPE=Release \
    -G Ninja

echo "=== Compilando RERev2 para ARM64 ==="
cmake --build "${BUILD_DIR}" --target RERev2 -j4

# 3. Copiar bibliotecas de runtime ARM64 para o diretório de build
if [[ -d "${RUNTIME_DIR}" ]]; then
    cp -d "${RUNTIME_DIR}"/* "${BUILD_DIR}/" 2>/dev/null || true
fi

echo "=== Compilação concluída com sucesso! ==="
echo "Binário gerado em: ${BUILD_DIR}/RERev2"
file "${BUILD_DIR}/RERev2" || true
