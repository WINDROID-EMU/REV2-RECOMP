#!/usr/bin/env bash
# ==============================================================================
# Resident Evil: Revelations 2 (REV2-RECOMP) - Native Game Launcher
# ==============================================================================

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
if [[ "$(uname -m)" == "aarch64" || "$(uname -m)" == "arm64" || "$*" == *"--arm64"* ]]; then
    BUILD_DIR="${SCRIPT_DIR}/build-arm64"
    RUNTIME_DIR="${SCRIPT_DIR}/runtime/linux-arm64"
else
    BUILD_DIR="${SCRIPT_DIR}/build"
    RUNTIME_DIR="${SCRIPT_DIR}/runtime/linux-amd64"
fi
DEFAULT_GAME_ROOT="${SCRIPT_DIR}/../extracted"

# 1. Locate game data root
GAME_DATA_ROOT=""
for arg in "$@"; do
    if [[ "$arg" == --game_data_root=* ]]; then
        GAME_DATA_ROOT="${arg#*=}"
    fi
done

if [[ -z "$GAME_DATA_ROOT" ]]; then
    if [[ -d "$DEFAULT_GAME_ROOT" ]]; then
        GAME_DATA_ROOT="$DEFAULT_GAME_ROOT"
    elif [[ -d "${SCRIPT_DIR}/extracted" ]]; then
        GAME_DATA_ROOT="${SCRIPT_DIR}/extracted"
    elif [[ -d "${SCRIPT_DIR}/game_data" ]]; then
        GAME_DATA_ROOT="${SCRIPT_DIR}/game_data"
    else
        echo "======================================================================"
        echo "AVISO: Nenhum diretório de dados do jogo especificado."
        echo "Uso: ./run.sh --game_data_root \"/caminho/para/assets_extraidos\""
        echo "======================================================================"
        exit 1
    fi
fi

# 2. Check executable
EXECUTABLE="${BUILD_DIR}/RERev2"
if [[ ! -f "$EXECUTABLE" ]]; then
    echo "Executável RERev2 não encontrado em ${BUILD_DIR}."
    echo "Compilando o projeto agora..."
    cmake -B "$BUILD_DIR" -DCMAKE_CXX_COMPILER=clang++
    cmake --build "$BUILD_DIR" --target RERev2 -j"$(nproc)"
    cp -d "${RUNTIME_DIR}"/* "${BUILD_DIR}/" 2>/dev/null || true
fi

# 3. Ensure runtime libraries are present in build directory
if [[ -d "$RUNTIME_DIR" && -d "$BUILD_DIR" ]]; then
    cp -d "${RUNTIME_DIR}"/* "${BUILD_DIR}/" 2>/dev/null || true
fi

# 4. Export runtime library paths
export LD_LIBRARY_PATH="${BUILD_DIR}:${RUNTIME_DIR}:${LD_LIBRARY_PATH}"
export DISPLAY="${DISPLAY:-:0}"

# 4.1 Audio buffer & driver settings (prevents audio crackling / buffer underrun)
export SDL_AUDIO_DEVICE_SAMPLE_FRAMES="${SDL_AUDIO_DEVICE_SAMPLE_FRAMES:-2048}"

mkdir -p "${SCRIPT_DIR}/cache/shaders/shareable" "${SCRIPT_DIR}/user_data"
rm -f /dev/shm/xenia_memory_* 2>/dev/null || true

# Garante que os shaders pré-compilados do jogo estejam no cache do desktop
if [[ -d "${SCRIPT_DIR}/android/app/src/main/assets/shaders/shareable" ]]; then
    cp -u "${SCRIPT_DIR}/android/app/src/main/assets/shaders/shareable"/5841144C* "${SCRIPT_DIR}/cache/shaders/shareable/" 2>/dev/null || true
fi

echo "Iniciando Resident Evil: Revelations 2 (REV2-RECOMP)..."
echo "  Binário:    ${EXECUTABLE}"
echo "  Game Root:  ${GAME_DATA_ROOT}"
echo "  Cache:      ${SCRIPT_DIR}/cache"
echo "  User Data:  ${SCRIPT_DIR}/user_data"
echo "  GPU Plugin: xenos (Vulkan)"
echo "  Otimizações: async_skip=true, readback=none, cache=1024MB"

ARGS=(--game_data_root "${GAME_DATA_ROOT}")
if [[ "$*" != *"--gpu_plugin"* ]]; then
    ARGS+=(--gpu_plugin "xenos")
fi
if [[ "$*" != *"--cache_root"* ]]; then
    ARGS+=(--cache_root "${SCRIPT_DIR}/cache")
fi
if [[ "$*" != *"--user_data_root"* ]]; then
    ARGS+=(--user_data_root "${SCRIPT_DIR}/user_data")
fi
if [[ "$*" != *"--readback_resolve"* ]]; then
    ARGS+=(--readback_resolve "none")
fi
if [[ "$*" != *"--vulkan_async_skip_incomplete_frames"* ]]; then
    ARGS+=(--vulkan_async_skip_incomplete_frames "true")
fi
if [[ "$*" != *"--async_shader_compilation"* ]]; then
    ARGS+=(--async_shader_compilation "true")
fi
if [[ "$*" != *"--log_level"* ]]; then
    ARGS+=(--log_level "warn")
fi
if [[ "$*" != *"--log_noisy"* ]]; then
    ARGS+=(--log_noisy "false")
fi
if [[ "$*" != *"--log_verbose"* ]]; then
    ARGS+=(--log_verbose "false")
fi

exec "${EXECUTABLE}" "${ARGS[@]}" "$@"

