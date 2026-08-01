#!/bin/sh

set -eu

usage()
{
    echo "Usage: $0 INPUT_VIDEO [OUTPUT_DIRECTORY]" >&2
    echo "Creates NAME.mpg and NAME.bmp for CrazyPod's /Videos folder." >&2
    exit 2
}

[ "$#" -ge 1 ] && [ "$#" -le 2 ] || usage

input=$1
output_directory=${2:-Videos}

[ -f "$input" ] || {
    echo "Input file does not exist: $input" >&2
    exit 1
}
command -v ffmpeg >/dev/null 2>&1 || {
    echo "ffmpeg is required." >&2
    exit 1
}
command -v ffprobe >/dev/null 2>&1 || {
    echo "ffprobe is required." >&2
    exit 1
}

filename=${input##*/}
name=${filename%.*}
[ -n "$name" ] || {
    echo "Input filename must contain a non-empty name." >&2
    exit 1
}

mkdir -p "$output_directory"
video_output=$output_directory/$name.mpg
poster_output=$output_directory/$name.bmp
video_temporary=$output_directory/.$name.crazypod-video.tmp
poster_temporary=$output_directory/.$name.crazypod-poster.tmp

cleanup()
{
    rm -f "$video_temporary" "$poster_temporary"
}
trap cleanup EXIT HUP INT TERM

ffmpeg -hide_banner -loglevel error -y \
    -i "$input" \
    -map 0:v:0 -map 0:a:0? \
    -vf "scale=320:240:force_original_aspect_ratio=increase,crop=320:240,setsar=1" \
    -r 24 \
    -c:v mpeg2video -pix_fmt yuv420p -q:v 5 -g 12 -bf 2 \
    -c:a mp2 -b:a 128k -ar 44100 -ac 2 \
    -f mpeg "$video_temporary"

video_codec=$(ffprobe -v error -select_streams v:0 \
    -show_entries stream=codec_name \
    -of default=noprint_wrappers=1:nokey=1 "$video_temporary")
video_width=$(ffprobe -v error -select_streams v:0 \
    -show_entries stream=width \
    -of default=noprint_wrappers=1:nokey=1 "$video_temporary")
video_height=$(ffprobe -v error -select_streams v:0 \
    -show_entries stream=height \
    -of default=noprint_wrappers=1:nokey=1 "$video_temporary")
duration=$(ffprobe -v error \
    -show_entries format=duration \
    -of default=noprint_wrappers=1:nokey=1 "$video_temporary")

[ "$video_codec" = "mpeg2video" ] || {
    echo "Conversion validation failed: video codec is $video_codec." >&2
    exit 1
}
[ "$video_width" = "320" ] && [ "$video_height" = "240" ] || {
    echo "Conversion validation failed: video size is ${video_width}x${video_height}." >&2
    exit 1
}
awk -v value="$duration" 'BEGIN { exit !(value > 0) }' || {
    echo "Conversion validation failed: video has no duration." >&2
    exit 1
}

poster_time=$(awk -v value="$duration" \
    'BEGIN { if (value > 2) printf "%.3f", value * 0.2; else print "0" }')
ffmpeg -hide_banner -loglevel error -y \
    -ss "$poster_time" -i "$input" \
    -frames:v 1 \
    -vf "scale=128:96:force_original_aspect_ratio=increase,crop=128:96" \
    -c:v bmp -f image2 "$poster_temporary"

[ -s "$poster_temporary" ] || {
    echo "Poster generation failed." >&2
    exit 1
}

mv -f "$video_temporary" "$video_output"
mv -f "$poster_temporary" "$poster_output"
trap - EXIT HUP INT TERM

echo "Video:  $video_output"
echo "Poster: $poster_output"
