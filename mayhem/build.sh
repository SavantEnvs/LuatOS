#!/usr/bin/env bash
#
# mayhem/build.sh — build the LuatOS fuzz harnesses and the known-answer oracle.
#
# Runs inside the commit image (mayhem/Dockerfile) as `mayhem` in /mayhem. xmake (upstream's build
# system) is not in the base image and its PC target pulls in the whole simulator, so the components
# that read untrusted data are compiled directly, twice:
#   mayhem/build/san  — $SANITIZER_FLAGS + $DEBUG_FLAGS (+ fuzzer-no-link): linked into every harness
#   mayhem/build/norm — upstream's normal -O2, no sanitizers: linked into the KAT oracle (mayhem/kat.c)
# Everything is built from the checkout; nothing is fetched, so the script re-runs offline.
set -euo pipefail

[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH

: "${SANITIZER_FLAGS=-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer}"
: "${DEBUG_FLAGS:=-g -gdwarf-3}"
: "${CC:=clang}" ; : "${CXX:=clang++}" ; : "${LIB_FUZZING_ENGINE:=-fsanitize=fuzzer}"
: "${MAYHEM_JOBS:=$(nproc)}"
: "${COVERAGE_FLAGS=}"
export SANITIZER_FLAGS DEBUG_FLAGS CC CXX LIB_FUZZING_ENGINE MAYHEM_JOBS COVERAGE_FLAGS

cd "$SRC"
B="$SRC/mayhem/build"
mkdir -p "$B/san" "$B/norm"

# Host defines/includes taken from bsp/pc/xmake.lua (the PC simulator target), minus the network stack.
DEFS="-D__LUATOS__ -D__XMAKE_BUILD__ -DLUA_USE_LINUX -DLUAT_CONF_USE_LIBSYS_SOURCE -DLUAT_BSP_PC"
INCS=""
for d in bsp/pc/include lua/include luat/include bsp/pc/port/posix components/printf components/common \
         components/network/adapter components/ethernet/common components/lua-cjson components/minmea \
         components/miniz components/network/http_parser components/network/libemqtt \
         components/network/httpsrv/inc components/sms/include components/pgfs \
         components/network/websocket components/fastlz components/serialization/protobuf \
         components/mbedtls/include components/network/lwip22/include components/network/ercoap/include \
         components/tjpgd components/cjson components/nanopb/include components/lcd components/u8g2 \
         components/iconv components/ymodem components/network/libhttp components/network/netdrv/include components/luat_image/include \
         components/luat_image/src components/libwebp components/libwebp/include \
         components/multimedia components/multimedia/audio/include components/multimedia/mp3_decode \
         components/multimedia/amr_decode/amr_common/dec/include components/multimedia/amr_decode/amr_nb/common/include \
         components/multimedia/amr_decode/amr_nb/dec/include components/multimedia/amr_decode/amr_wb/dec/include \
         components/multimedia/amr_decode/amr_wb/enc/include components/multimedia/amr_decode/amr_wb/enc/common/include \
         components/multimedia/amr_decode/opencore-amrnb components/multimedia/amr_decode/opencore-amrwb \
         components/multimedia/amr_decode/oscl components/multimedia/amr_decode/amr_nb/enc/src \
         components/multimedia/dtmf_codec components/multimedia/vtool/include components/speex/include mayhem; do
  INCS="$INCS -I$SRC/$d"
done

SRCS="$(ls lua/src/*.c | grep -v -e '/bget\.c$' -e '/luat_bget\.c$')
components/printf/printf.c components/printf/printf_dtoa.c
luat/modules/luat_base.c luat/vfs/luat_vfs.c luat/vfs/luat_fs_inline.c
luat/vfs/luat_inline_libs.c luat/vfs/luat_inline_libs_source.c
bsp/pc/port/luat_log_mini.c
components/lua-cjson/lua_cjson.c components/lua-cjson/fpconv.c components/lua-cjson/strbuf.c
components/minmea/minmea.c components/sms/src/luat_sms.c
components/network/httpsrv/src/luat_httpsrv_http.c components/network/http_parser/http_parser.c
components/miniz/miniz.c components/miniz/luat_lib_miniz.c
components/minmea/luat_gnss.c components/network/websocket/luat_websocket.c
components/fastlz/fastlz.c components/fastlz/luat_lib_fastlz.c
luat/modules/luat_lib_pack.c components/serialization/protobuf/luat_lib_protobuf.c
components/network/ercoap/src/er-coap-13.c components/network/ercoap/binding/luat_lib_ercoap.c
components/tjpgd/tjpgd.c components/cjson/cJSON.c components/cjson/cJSON_Utils.c
$(ls components/nanopb/src/*.c)
$(ls components/iconv/*.c) components/ymodem/luat_ymodem.c
components/network/libemqtt/libemqtt.c components/network/libemqtt/luat_mqtt_client.c
components/network/libhttp/luat_http_client.c components/network/libhttp/luat_http_ctrl.c components/common/c_common.c
components/luat_image/src/luat_image_webp.c components/luat_image/src/luat_image_common.c
$(ls components/libwebp/src/dec/*.c components/libwebp/src/dsp/*.c components/libwebp/src/utils/*.c)
components/multimedia/luat_lib_multimedia_codec.c components/multimedia/mp3_decode_port.c components/multimedia/minimp3.c
components/multimedia/amr_codec_port.c components/multimedia/wav_codec_port.c components/multimedia/speex_codec_port.c
components/multimedia/g711_codec_port.c components/multimedia/g711_codec/g711_codec.c components/multimedia/dtmf_codec/dtmf_codec.c
$(find components/multimedia/amr_decode -name '*.c' | sort) $(ls components/speex/libspeex/*.c) luat/vfs/luat_fs_mem.c
mayhem/support.c mayhem/support_http.c"

# Per-file UBSan relaxations for the sanitized build (never an ignorelist: ccache does not hash it).
#  minmea.c: `scale *= 10` in minmea_scan() overflows `int` on a long fraction ("1.00000000000...").
#  luat_lib_protobuf.c: pb_addslice() does memcpy(buff, NULL, 0) for an empty slice (nonnull-attribute; a no-op in practice),
#    and lpbE_enum() casts a negative lua_Number to uint64_t (float-cast-overflow; yields the intended 10-byte varint on x86).
#  tjpgd.c: the IDCT multiplies dequantised coefficients by fixed-point constants (`(v1 - v3) * M13 >> 12`) in int32, which
#    overflows for extreme coefficients; the result only reaches the output through BYTECLIP's `& 0x3FF` table index (signed-integer-overflow).
#  libwebp dsp/dec.c: the inverse-transform macros `(a) * 35468 >> 16` multiply a dequantised coefficient in int32, which overflows for
#    extreme coefficients; the wrapped value only feeds the clipped pixel sums (signed-integer-overflow).
#  fuzz_minmea.c: minmea_tocoord2() (inline in minmea.h) computes `degrees * 10000000` in int32 (signed-integer-overflow, then
#    stored to uint32_t) and casts a negative float to uint32_t (float-cast-overflow, only via a '-' in a coordinate field).
#  utf8_to_ucs2.c: enc_get_utf8_size() shifts a negative `char` left (`c << 1`) on every non-ASCII UTF-8 byte
#    (shift-base); the result is the intended byte shift, so only that check is relaxed for the file.
#  amr_decode/ (opencore AMR-NB/WB): the ETSI fixed-point basic_op idiom (L_add, L_shl, `x << n` on negative values) relies on
#    two's-complement wrap and negative shifts by design (signed-integer-overflow, shift-base, shift-exponent).
#  speex/libspeex: the fixed-point filters (filters.c, ltp.c) overflow int32 in intermediate sums by design (signed-integer-overflow).
#  The codec targets drive the firmware's own `codec` Lua API over the vfs RAM filesystem (luat_fs_mem.c, mount /ram/).
#  libwebp (the PC firmware's file set: dec/, dsp/, utils/) and luat_image_webp.c are built with LUAT_USE_WEBP=1 as bsp/pc/xmake.lua
#    does; WEBP_MAX_ALLOCABLE_MEMORY (libwebp's own build option) is lowered to the 64 MB heap cap of mayhem/support.c, so a header
#    that claims a huge image fails to decode, as it does on a device, instead of allocating gigabytes.
#  luat_ymodem.c is built in the library's callback mode (-U__LUATOS__): received data goes to a callback
#    instead of luat_fs_*, so the harness never touches a filesystem. The parsing code is the same.
defs_for() {
  case "$1" in
    components/ymodem/luat_ymodem.c|mayhem/fuzz_ymodem.c|mayhem/kat_ymodem.c) echo "-U__LUATOS__" ;;
    components/libwebp/*|components/luat_image/src/*|mayhem/fuzz_webp.c|mayhem/kat_webp.c) echo "-DLUAT_USE_WEBP=1 -DWEBP_MAX_ALLOCABLE_MEMORY=67108864" ;;
  esac
}
relax_for() {
  case "$1" in
    components/minmea/minmea.c|components/tjpgd/tjpgd.c|components/libwebp/src/dsp/dec.c) echo "-fno-sanitize=signed-integer-overflow" ;;
    components/serialization/protobuf/luat_lib_protobuf.c) echo "-fno-sanitize=nonnull-attribute,float-cast-overflow" ;;
    components/iconv/utf8_to_ucs2.c) echo "-fno-sanitize=shift-base" ;;
    mayhem/fuzz_minmea.c) echo "-fno-sanitize=float-cast-overflow,signed-integer-overflow" ;;
    components/multimedia/amr_decode/*) echo "-fno-sanitize=shift-base,shift-exponent,signed-integer-overflow" ;;
    components/speex/libspeex/*) echo "-fno-sanitize=signed-integer-overflow" ;;
  esac
}

compile_one() {  # <variant> <src>
  local v="$1" s="$2" o="$B/$1/$(echo "$2" | tr / _).o"
  if [ "$v" = san ]; then
    $CC $SANITIZER_FLAGS -fsanitize=fuzzer-no-link $(relax_for "$s") $DEBUG_FLAGS -O1 -w $DEFS $(defs_for "$s") $INCS -c "$SRC/$s" -o "$o"
  else
    $CC -O2 -g0 -w $DEFS $(defs_for "$s") $INCS $COVERAGE_FLAGS -c "$SRC/$s" -o "$o"
  fi
}
export -f compile_one relax_for defs_for
export B SRC DEFS INCS

for v in san norm; do
  printf '%s\n' $SRCS | xargs -P "$MAYHEM_JOBS" -I{} bash -c "compile_one $v {}"
  rm -f "$B/$v/libluatos.a"
  ar rcs "$B/$v/libluatos.a" "$B/$v"/*.o
done

# $LIB_FUZZING_ENGINE carries the libFuzzer runtime; lsan_off.o keeps ASan but turns leak checking off.
$CXX $SANITIZER_FLAGS $DEBUG_FLAGS -c "$SRC/mayhem/lsan_off.cc" -o "$B/lsan_off.o"
$CC $SANITIZER_FLAGS $DEBUG_FLAGS -c "$STANDALONE_FUZZ_MAIN" -o "$B/standalone_main.o"

# Without ASan nothing provides the SanCov callbacks the instrumented library needs, so link LLVM's
# main-less libFuzzer runtime into the standalone reproducers (the sanitizer-off build).
STANDALONE_RT=""
case "$SANITIZER_FLAGS" in
  *address*) ;;
  *) STANDALONE_RT="$($CC --print-file-name=libclang_rt.fuzzer_no_main-$(uname -m).a)" ;;
esac

HARNESSES="fuzz_lua_load fuzz_lua_cjson fuzz_minmea fuzz_sms_pdu fuzz_httpsrv_request fuzz_miniz fuzz_miniz_zip
           fuzz_protobuf fuzz_fastlz fuzz_pack fuzz_websocket fuzz_libgnss fuzz_ercoap fuzz_cjson fuzz_tjpgd fuzz_nanopb fuzz_ymodem fuzz_iconv fuzz_mqtt
           fuzz_http_client fuzz_webp fuzz_codec_mp3 fuzz_codec_wav fuzz_codec_amr fuzz_codec_speex"
for h in $HARNESSES; do
  $CC $SANITIZER_FLAGS -fsanitize=fuzzer-no-link $(relax_for "mayhem/$h.c") $DEBUG_FLAGS -O1 -w $DEFS $(defs_for "mayhem/$h.c") $INCS -c "$SRC/mayhem/$h.c" -o "$B/$h.o"
  $CC $SANITIZER_FLAGS $DEBUG_FLAGS $LIB_FUZZING_ENGINE "$B/$h.o" "$B/san/libluatos.a" "$B/lsan_off.o" -lm -ldl -lpthread -o "/mayhem/$h"
  $CC $SANITIZER_FLAGS $DEBUG_FLAGS "$B/standalone_main.o" "$B/$h.o" "$B/san/libluatos.a" "$B/lsan_off.o" $STANDALONE_RT -lstdc++ -lm -ldl -lpthread -o "/mayhem/$h-standalone"
done

# Known-answer oracle: upstream sources at normal flags, dynamically linked (test.sh asserts its output).
for k in kat kat_ymodem kat_mqtt kat_webp kat_codec; do
  $CC -O2 -g0 -w $DEFS $(defs_for "mayhem/$k.c") $INCS $COVERAGE_FLAGS -c "$SRC/mayhem/$k.c" -o "$B/$k.o"
done
$CC -O2 $COVERAGE_FLAGS "$B/kat.o" "$B/kat_ymodem.o" "$B/kat_mqtt.o" "$B/kat_webp.o" "$B/kat_codec.o" "$B/norm/libluatos.a" -lm -ldl -lpthread -o "/mayhem/luatos_kat"
file /mayhem/luatos_kat | grep -q 'dynamically linked'
