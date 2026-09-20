#!/bin/zsh
set -e

SCRIPT_DIR="${0:A:h}"
PROJECT_ROOT="${SCRIPT_DIR:h:h}"
RENDERER="$PROJECT_ROOT/build-offline/fform-offline-renderer"

if [[ ! -x "$RENDERER" ]]; then
  echo "No se encuentra el renderer Release: $RENDERER"
  echo "Compila primero con FFORM_BUILD_OFFLINE_HARNESS=ON."
  exit 1
fi

printf 'Ruta del WAV de entrada: '
read input
if [[ ! -f "$input" ]]; then
  echo "No existe el archivo: $input"
  exit 1
fi

printf 'Time ratio [1.0]: '
read time_ratio
: ${time_ratio:=1.0}
printf 'Pitch en semitonos [0]: '
read pitch
: ${pitch:=0}
printf 'Bloque [4096]: '
read block_size
: ${block_size:=4096}

name="$(basename "$input" .wav)"
out_dir="$(dirname "$input")/F-Form Offline Renders"
mkdir -p "$out_dir"
output="$out_dir/${name}_time-${time_ratio}_pitch-${pitch}.wav"
metrics="$out_dir/${name}_time-${time_ratio}_pitch-${pitch}.json"

"$RENDERER" --mode streaming --block-size "$block_size" \
  --input "$input" --output "$output" --metrics "$metrics" \
  --time-ratio "$time_ratio" --pitch-semitones "$pitch"

printf '\nRender creado:\n%s\n' "$output"
printf 'Abrir carpeta de resultados? [y/N] '
read open_folder
if [[ "$open_folder" == "y" || "$open_folder" == "Y" ]]; then
  open "$out_dir"
fi
