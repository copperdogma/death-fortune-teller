# Audio Specifications for Death Fortune Teller

## WAV File Format Requirements

All WAV files for the Death Fortune Teller project must be formatted as:

**Format**: PCM (WAV)  
**Sample Rate**: 44.1 kHz (44,100 Hz)  
**Bit Depth**: 16-bit  
**Channels**: 2 (stereo)  

## File Conversion Details

### Naming Convention
- Remove any "Death - " prefix
- Convert to lowercase
- Remove special characters
- Replace spaces with underscores

### Conversion Process

**Input files**: M4A format (AAC audio codec)
**Output format**: PCM WAV, 44.1 kHz, 16-bit, stereo
**Processing**: Automatically trims start/end silence and applies peak normalization

**FFmpeg command template**:
```bash
ffmpeg -y -i "input.m4a" -af "alimiter=level_in=1:level_out=0.95:limit=0.95,silenceremove=start_periods=1:start_threshold=-30dB:start_duration=1.0,areverse,silenceremove=start_periods=1:start_threshold=-30dB:start_duration=1.0,areverse,aformat=s16:44100:stereo" -ar 44100 -ac 2 -sample_fmt s16 -acodec pcm_s16le "output.wav"
```

**Note**: The `-y` flag automatically overwrites existing output files without prompting.

**Automated conversion script**:
Use the provided `convert_audio.sh` script for reliable, error-free conversions:

```bash
# Convert a single file
./convert_audio.sh "input_file.m4a"

# Convert all audio files in a directory
./convert_audio.sh "/path/to/audio/folder"

# Convert all files in current directory
./convert_audio.sh "."
```

**Manual FFmpeg command** (for reference):
```bash
ffmpeg -y -i "input.m4a" -af "alimiter=level_in=1:level_out=0.95:limit=0.95,silenceremove=start_periods=1:start_threshold=-30dB:start_duration=1.0,areverse,silenceremove=start_periods=1:start_threshold=-30dB:start_duration=1.0,areverse,aformat=s16:44100:stereo" -ar 44100 -ac 2 -sample_fmt s16 -acodec pcm_s16le "output.wav"
```

### Audio Processing Details

**⚠️ IMPORTANT: The conversion automatically applies two processing steps in the correct order:**

1. **Peak Normalization (FIRST)**:
   - Applies automatic gain control to maximize volume
   - Limits output to 95% of maximum to prevent clipping
   - **Why first**: Boosts quiet speech levels so they won't be mistaken for silence
   - **Result**: All files will have consistent, optimized volume levels

2. **Silence Trimming (SECOND)**:
   - Removes silence from the beginning and end of each file using the `areverse` technique
   - Uses -30dB threshold for silence detection (much more conservative)
   - Requires 1.0 seconds of non-silence to stop trimming (very conservative)
   - Uses `areverse` to apply trimming to both start and end of audio
   - **Result**: Files will be shorter than originals if they had leading/trailing silence

**Why this matters**: Your converted files may sound different from the originals because:
- Leading/trailing silence has been removed (files may be shorter)
- Volume levels have been normalized (may sound louder/quieter)
- This ensures consistent playback quality on the ESP32 system

## Technical Details

This format is hardcoded into the audio player implementation in `src/audio_player.h`:

```cpp
// Hardcoded audio format specifications
static constexpr uint32_t AUDIO_SAMPLE_RATE = 44100;
static constexpr uint8_t AUDIO_BIT_DEPTH = 16;
static constexpr uint8_t AUDIO_NUM_CHANNELS = 2;
```

The audio player skips the WAV header (seeking to byte 128) and expects this specific format for proper playback.

## Usage Notes

**Supported input formats:**
- M4A (AAC audio codec)
- MP3
- WAV (will be re-encoded to required format)
- Most common audio formats supported by FFmpeg

**Output verification:**
After conversion, verify files meet ESP32 requirements:
- ✅ PCM signed 16-bit little-endian
- ✅ 44.1 kHz sample rate
- ✅ 2 channels (stereo)
- ✅ Ready for ESP32 deployment

**File organization:**
Converted WAV files should be organized into the appropriate SD card directory structure based on their content and intended use in the Death Fortune Teller system.

