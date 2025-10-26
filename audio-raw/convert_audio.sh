#!/bin/bash

# Audio Conversion Script for Death Fortune Teller
# Converts audio files to ESP32-compatible WAV format with silence trimming and peak normalization

set -e  # Exit on any error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Function to print colored output
print_status() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Function to show usage
show_usage() {
    echo "Usage: $0 <input_path>"
    echo ""
    echo "Arguments:"
    echo "  input_path    Path to a single audio file or directory containing audio files"
    echo ""
    echo "Examples:"
    echo "  $0 audio_file.m4a"
    echo "  $0 /path/to/audio/folder"
    echo "  $0 ."
    echo ""
    echo "Supported input formats: M4A, MP3, WAV, and other formats supported by FFmpeg"
    echo "Output: PCM WAV, 44.1 kHz, 16-bit, stereo with silence trimming and peak normalization"
}

# Function to check if FFmpeg is available
check_ffmpeg() {
    if ! command -v ffmpeg &> /dev/null; then
        print_error "FFmpeg is not installed or not in PATH"
        print_error "Please install FFmpeg: https://ffmpeg.org/download.html"
        exit 1
    fi
}

# Function to convert a single file
convert_file() {
    local input_file="$1"
    local input_dir=$(dirname "$input_file")
    local input_basename=$(basename "$input_file")
    
    print_status "Converting: $input_basename"
    
    # Apply naming convention: remove "Death - " prefix, lowercase, replace spaces with underscores
    local basename_no_ext="${input_basename%.*}"
    local newname=$(echo "$basename_no_ext" | sed 's/^Death - //' | tr '[:upper:]' '[:lower:]' | sed 's/ /_/g')
    local final_output="${input_dir}/${newname}.wav"
    
    # FFmpeg command with proper normalization first, then detect speech boundaries and trim with original audio buffer
    if ffmpeg -y -i "$input_file" \
        -af "loudnorm=I=-16:TP=-1.5:LRA=11,alimiter=level_in=1:level_out=0.95:limit=0.95,silenceremove=start_periods=1:start_threshold=-30dB:start_duration=0.1:start_silence=0.5,areverse,silenceremove=start_periods=1:start_threshold=-30dB:start_duration=0.1:start_silence=0.5,areverse,aformat=s16:44100:stereo" \
        -ar 44100 -ac 2 -sample_fmt s16 -acodec pcm_s16le \
        "$final_output" 2>/dev/null; then
        
        print_success "Converted: $input_basename -> $(basename "$final_output") (trimmed silence, peak normalized)"
        return 0
    else
        print_error "Failed to convert: $input_basename"
        return 1
    fi
}

# Function to process a directory
process_directory() {
    local dir_path="$1"
    local converted_count=0
    local failed_count=0
    
    print_status "Processing directory: $dir_path"
    
    # Find audio files (common extensions)
    while IFS= read -r -d '' file; do
        local filename=$(basename "$file")
        local extension="${filename##*.}"
        
        # Check if it's an audio file and not already a converted WAV
        if [[ "$extension" =~ ^(m4a|mp3|aac|flac|ogg|wma)$ ]]; then
            if convert_file "$file"; then
                ((converted_count++))
            else
                ((failed_count++))
            fi
        elif [[ "$extension" == "wav" ]]; then
            print_warning "Skipping already converted WAV file: $filename"
        else
            print_warning "Skipping non-audio file: $filename"
        fi
    done < <(find "$dir_path" -maxdepth 1 -type f -print0)
    
    echo ""
    print_status "Directory processing complete:"
    print_success "Successfully converted: $converted_count files"
    if [ $failed_count -gt 0 ]; then
        print_error "Failed conversions: $failed_count files"
    fi
}

# Function to process a single file
process_single_file() {
    local file_path="$1"
    
    if [ ! -f "$file_path" ]; then
        print_error "File not found: $file_path"
        exit 1
    fi
    
    local filename=$(basename "$file_path")
    local extension="${filename##*.}"
    
    # Check if it's an audio file
    if [[ ! "$extension" =~ ^(m4a|mp3|wav|aac|flac|ogg|wma)$ ]]; then
        print_error "Unsupported file format: $extension"
        print_error "Supported formats: m4a, mp3, wav, aac, flac, ogg, wma"
        exit 1
    fi
    
    print_status "Processing single file: $filename"
    
    if convert_file "$file_path" "$filename"; then
        print_success "Conversion completed successfully"
    else
        print_error "Conversion failed"
        exit 1
    fi
}

# Main script logic
main() {
    # Check arguments
    if [ $# -eq 0 ]; then
        print_error "No input path provided"
        echo ""
        show_usage
        exit 1
    fi
    
    if [ "$1" = "-h" ] || [ "$1" = "--help" ]; then
        show_usage
        exit 0
    fi
    
    local input_path="$1"
    
    # Check if FFmpeg is available
    check_ffmpeg
    
    print_status "Death Fortune Teller Audio Converter"
    print_status "====================================="
    echo ""
    
    # Determine if input is file or directory
    if [ -f "$input_path" ]; then
        process_single_file "$input_path"
    elif [ -d "$input_path" ]; then
        process_directory "$input_path"
    else
        print_error "Path not found: $input_path"
        print_error "Please provide a valid file or directory path"
        exit 1
    fi
    
    echo ""
    print_success "All conversions completed!"
    print_status "Converted files are ready for ESP32 deployment"
}

# Run main function with all arguments
main "$@"
