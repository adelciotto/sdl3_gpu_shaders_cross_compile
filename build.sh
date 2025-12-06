#!/bin/bash
set -e
cd "$(dirname "${BASH_SOURCE[0]}")"
source_dir="$(pwd)"

# --- Build SDL3 If Needed ---------------------------------------------------
sdl3_version="3.2.28"
sdl3_url="https://github.com/libsdl-org/SDL/releases/download/release-${sdl3_version}/SDL3-${sdl3_version}.tar.gz"
sdl3_install_dir="${source_dir}/extern/SDL3/linux"
sdl3_build_dir="${source_dir}/tmp/sdl3_build"

if [ ! -d "$sdl3_install_dir" ]; then
  echo "Building SDL3 ${sdl3_version}..."
  mkdir -p "$sdl3_build_dir" "$sdl3_install_dir"
  cd "$sdl3_build_dir"
  wget -O SDL3.tar.gz "$sdl3_url"
  tar -xzf SDL3.tar.gz
  cd "SDL3-${sdl3_version}"
  mkdir -p build
  cd build
  cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="$sdl3_install_dir" \
    -DBUILD_SHARED_LIBS=ON
  cmake --build . --config Release --parallel $(nproc)
  cmake --install .
  cd "$source_dir"
  rm -rf tmp
  echo "SDL3 installed to ${sdl3_install_dir}"
else
  echo "SDL3 already present at ${sdl3_install_dir}"
fi

cd "$source_dir"

# --- Unpack Arguments -------------------------------------------------------
debug=0
release=0
for arg in "$@"; do
  if [ "$arg" == "debug" ]; then debug=1; fi
  if [ "$arg" == "release" ]; then release=1; fi
done
if [ $release -ne 1 ]; then debug=1; fi
if [ $debug -eq 1 ]; then release=0 && echo "[debug mode]"; fi
if [ $release -eq 1 ]; then debug=0 && echo "[release mode]"; fi

# --- Unpack Command line Build Arguments ------------------------------------
# None for now...

# --- Compile/Link Definitions -----------------------------------------------
cc_common="-std=c++17 \
           -I../src -I../extern/HandmadeMath -I../extern/SDL3/linux/include -I../extern/imgui"
cc_debug="g++ -g -O0 -DBUILD_DEBUG -DRESOURCES_PATH="\"${source_dir}/"\" -I../extern/SDL3_shadercross/linux/include
${cc_common}"
cc_release="g++ -O2 ${cc_common}"
cc_link_common="-L../extern/SDL3/linux/lib -lSDL3 -Wl,-rpath,\$ORIGIN"
cc_link_debug="-L../extern/SDL3_shadercross/linux/lib -lSDL3_shadercross ${cc_link_common}"
cc_link_release="${cc_link_common}"
if [ $debug -eq 1 ]; then cc_compile="$cc_debug"; fi
if [ $release -eq 1 ]; then cc_compile="$cc_release"; fi
if [ $debug -eq 1 ]; then cc_link="$cc_link_debug"; fi
if [ $release -eq 1 ]; then cc_link="$cc_link_release"; fi

# --- Shader Compile Definitions ---------------------------------------------
shadercross="../extern/SDL3_shadercross/linux/bin/shadercross"
shadercross_vertex="$shadercross -t vertex -DVERTEX_SHADER"
shadercross_fragment="$shadercross -t fragment -DFRAGMENT_SHADER"

# --- Prep Directories -------------------------------------------------------
build_dir_debug="build_debug"
build_dir_release="build_release"
if [ $debug -eq 1 ]; then build_dir="$build_dir_debug"; fi
if [ $release -eq 1 ]; then build_dir="$build_dir_release"; fi
mkdir -p "$build_dir"
mkdir -p "$build_dir/res"

# --- Build Everything -------------------------------------------------------
pushd "$build_dir" >/dev/null

if [ $release -eq 1 ]; then
  echo "Compiling shaders..."
  $shadercross_vertex ../src/fullscreen.hlsl -o res/fullscreen.spv || exit 1
  $shadercross_fragment ../src/fbm_warp.hlsl -o res/fbm_warp.spv || exit 1
  $shadercross_fragment ../src/plasma_beat.hlsl -o res/plasma_beat.spv || exit 1
fi

echo "Compiling source files..."
$cc_compile ../src/sdl3_gpu_shaders_cross_compile.cpp \
  ../extern/imgui/imgui.cpp \
  ../extern/imgui/imgui_demo.cpp \
  ../extern/imgui/imgui_draw.cpp \
  ../extern/imgui/imgui_impl_sdl3.cpp \
  ../extern/imgui/imgui_impl_sdlgpu3.cpp \
  ../extern/imgui/imgui_tables.cpp \
  ../extern/imgui/imgui_widgets.cpp \
  $cc_link -o sdl3_gpu_shaders_cross_compile || exit 1

popd >/dev/null

# --- Copy .so's -------------------------------------------------------------
if [ ! -f "$build_dir/libSDL3.so.0" ]; then
  cp -t "$build_dir/" extern/SDL3/linux/lib/libSDL3.so.0* 2>/dev/null || true
fi
if [ $debug -eq 1 ]; then
  if [ ! -f "$build_dir/libSDL3_shadercross.so.0" ]; then
    cp -t "$build_dir/" extern/SDL3_shadercross/linux/lib/libSDL3_shadercross.so* 2>/dev/null || true
  fi
  if [ ! -f "$build_dir/libdxcompiler.so" ]; then
    cp -t "$build_dir/" extern/SDL3_shadercross/linux/lib/libdxcompiler.so 2>/dev/null || true
  fi
  if [ ! -f "$build_dir/libdxil.so" ]; then
    cp -t "$build_dir/" extern/SDL3_shadercross/linux/lib/libdxil.so 2>/dev/null || true
  fi
  if [ ! -f "$build_dir/libspirv-cross-c-shared.so.0" ]; then
    cp -t "$build_dir/" extern/SDL3_shadercross/linux/lib/libspirv-cross-c-shared.so* 2>/dev/null || true
  fi
  if [ ! -f "$build_dir/libvkd3d.so.1" ]; then
    cp -t "$build_dir/" extern/SDL3_shadercross/linux/lib/libvkd3d*.so* 2>/dev/null || true
  fi
fi

echo "Done!"
