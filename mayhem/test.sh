#!/usr/bin/env bash
#
# mayhem/test.sh — known-answer oracle for LuatOS (built by mayhem/build.sh as /mayhem/luatos_kat).
# LuatOS ships no host-runnable unit tests, so mayhem/kat.c drives the real library (Lua VM + cjson,
# minmea, sms PDU decoder, httpsrv request parser, miniz, fastlz, pack, protobuf, libgnss, ercoap, cJSON, nanopb, tjpgd, iconv, ymodem, mqtt, http client, webp, codec wav/mp3/amr) at normal flags and prints key=value lines;
# every line is compared here against a golden value, so a no-op or exit(0) build fails.
#
# IMPORTANT:
#  * Must assert BEHAVIOR/OUTPUT, not just exit status. The oracle has to check asserted values /
#    golden-output diffs / known-answer results — so a PATCH that "fixes" a bug by making the program
#    exit(0) (or any no-op) FAILS here. Running inputs and checking only "exit 0 / didn't crash" is
#    NOT a functional test (it's trivially reward-hackable) — use the project's real assertion suite.
#  * Do NOT build here — mayhem/build.sh already compiled the test suite (with the project's normal
#    flags). This script only RUNS the pre-built tests and reports counts. If the test runner is
#    missing, that's a build.sh bug — fail loudly rather than silently rebuilding.
#  * REQUIRED OUTPUT — a CTRF (https://ctrf.io) summary so Mayhem/the PATCH grader reads the counts:
#      - writes a CTRF JSON report to ${CTRF_REPORT:-$SRC/ctrf-report.json}, and
#      - prints a one-line `CTRF {...}` marker to stdout (same JSON, compact).
#    Only `results.summary` (with tests/passed/failed/pending/skipped/other) is required.
#    Use the emit_ctrf helper below; it computes tests = passed+failed+skipped and sets the exit
#    code (0 iff failed==0). Map your framework's output to passed/failed/skipped.
set -uo pipefail
[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH
: "${MAYHEM_JOBS:=$(nproc)}"   # build parallelism; env-overridable, falls back to nproc (use -j"$MAYHEM_JOBS")
cd "$SRC"

# emit_ctrf <tool> <passed> <failed> [skipped] [pending] [other]
# Writes a CTRF report (file + stdout `CTRF {...}` marker) and returns non-zero iff failed>0.
emit_ctrf() {
  local tool="$1" passed="$2" failed="$3" skipped="${4:-0}" pending="${5:-0}" other="${6:-0}"
  local tests=$(( passed + failed + skipped + pending + other ))
  cat > "${CTRF_REPORT:-$SRC/ctrf-report.json}" <<JSON
{
  "results": {
    "tool": { "name": "$tool" },
    "summary": {
      "tests": $tests,
      "passed": $passed,
      "failed": $failed,
      "pending": $pending,
      "skipped": $skipped,
      "other": $other
    }
  }
}
JSON
  printf 'CTRF {"results":{"tool":{"name":"%s"},"summary":{"tests":%d,"passed":%d,"failed":%d,"pending":%d,"skipped":%d,"other":%d}}}\n' \
    "$tool" "$tests" "$passed" "$failed" "$pending" "$skipped" "$other"
  [ "$failed" -eq 0 ]
}

KAT=/mayhem/luatos_kat
GOLDEN='lua_arith=42
cjson_decode=6
cjson_string=x!
cjson_bool=yes
cjson_encode=[10,20,30]
cjson_roundtrip=[1.5000000,"q"]
nmea_check=1
nmea_id=1
nmea_time=12:35:19
nmea_lat=4807038/1000
nmea_lon=1131000/1000
nmea_speed=224/10
nmea_date=23-03-94
nmea_badsum=0
sms_rc=0
sms_text=Hello
sms_from=86138001380
sms_time=26-11-21 12:34:56
sms_truncated_rc=-1
http_done=1
http_uri=/api/v1/set?x=1&y=2
http_method=POST
http_expect=5
http_body=hello
http_headers=3
miniz_inflate=hello
miniz_roundtrip=1
miniz_shrinks=1
fastlz_roundtrip=true
fastlz_literal=hello
pack_pack=010200010000
pack_unpack=7,258,65536
pb_encode_len=7
pb_decode=150,hi
coap_parse=0,1,4660,hi
gnss_rmc=$GPRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W*6A
gnss_split=1
cjson_print={"a":[1,2.5,"x"],"b":{"c":true,"d":null}}
cjson_reject=1
nanopb_decode=1 pin=5 mode=1 irq=2 debounce=20
nanopb_truncated=0
jpeg_prepare=0 size=64x64
jpeg_decomp=0 pixels=4096 sum=2021626
iconv_utf8_to_ucs2=0 604F7D59
iconv_gb2312_to_ucs2be=0 4F60597D
iconv_ucs2_to_utf8=0 E4BDA0E5A5BD
iconv_ucs2_to_gb2312=0 C4E3BAC3
iconv_unknown=1
ymodem_handshake=C
ymodem_header=0 ack=6 flag=C name=a.bin size=5
ymodem_badcrc=-1 ack=21
ymodem_data=0 part_ack=0 ack=6 file_ok=1 got=hello len=5
ymodem_eot1=21
ymodem_eot2=6 flag=C
ymodem_end=0 ack=6 all_done=1
mqtt_msg_id=4660
mqtt_read=0 connack=1 state=3 publish=1 topic=a/b payload=hello!!!
mqtt_bad_publish=-1 state=0
http_client_length=1 event=0 status=200 body_len=5 body=hello content_type=1
http_client_chunked=1 event=0 status=200 body_len=11 body=hello world content_type=1
http_client_notfound=1 event=0 status=404 body_len=4 body=gone content_type=1
webp_lossless=0 size=1x1 first_pixel_rgb565=0000
webp_lossy=0 size=1x1 first_pixel_rgb565=8410
webp_truncated=-1 size=0x0 first_pixel_rgb565=none
webp_garbage=-1 size=0x0 first_pixel_rgb565=none
codec_wav=info=1 channels=1 rate=8000 bits=16 calls=8 bytes=16000 sum=2038848
codec_mp3=info=1 channels=2 rate=44100 bits=16 calls=1 bytes=9216 sum=0
codec_amr_nb=info=1 channels=1 rate=8000 bits=16 calls=1 bytes=960 sum=113687
codec_amr_wb=info=1 channels=1 rate=16000 bits=16 calls=1 bytes=1920 sum=240468
codec_amr_bad_magic=info=0 channels=1 rate=16000 bits=16
kat_done=1'

passed=0; failed=0
if [ ! -x "$KAT" ]; then
  echo "FAIL: $KAT missing (mayhem/build.sh should have built it)" >&2
  emit_ctrf "luatos-kat" 0 1 0
  exit 1
fi
OUT="$(cd /tmp && "$KAT" 2>/dev/null)"
rc=$?
if [ "$rc" -ne 0 ]; then
  echo "FAIL: $KAT exited $rc" >&2
  failed=$((failed + 1))
fi
while IFS= read -r want; do
  key="${want%%=*}"
  got="$(printf '%s\n' "$OUT" | grep -a -m1 "^${key}=" || true)"
  if [ "$got" = "$want" ]; then
    passed=$((passed + 1))
  else
    echo "FAIL: $key: want '${want#*=}' got '${got#*=}'" >&2
    failed=$((failed + 1))
  fi
done <<< "$GOLDEN"

emit_ctrf "luatos-kat" "$passed" "$failed" 0
