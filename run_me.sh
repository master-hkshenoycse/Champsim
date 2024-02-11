#!/bin/bash


gap_dir="$1"


for fullfile in "$gap_dir"/*; do
    filename=$(basename -- "$fullfile")
    extension="${filename##*.}"
    filename="${filename%.*}"
    filename="${filename%.*}"
    echo $filename

    log_name="$2""/log_summary_${filename}"
    

    "$3" --warmup_instructions 40000000 --simulation_instructions 100000000 ${fullfile} > ${log_name} 
done;


