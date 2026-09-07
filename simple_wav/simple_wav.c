#include "simple_wav.h"
#include <errno.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ERROR_NAME_TABLE_ENTRY(e) [e] = #e
static const char *ERROR_NAME_TABLE[] = {
    ERROR_NAME_TABLE_ENTRY(SIMPLE_WAV_SUCCESS),
    ERROR_NAME_TABLE_ENTRY(SIMPLE_WAV_ERR_FILE_ERROR),
    ERROR_NAME_TABLE_ENTRY(SIMPLE_WAV_ERR_INVALID_ARG),
    ERROR_NAME_TABLE_ENTRY(SIMPLE_WAV_ERR_EOF),
    ERROR_NAME_TABLE_ENTRY(SIMPLE_WAV_ERR_UNSUPPORTED),
    ERROR_NAME_TABLE_ENTRY(SIMPLE_WAV_ERR_INVALID_HEADER),
    ERROR_NAME_TABLE_ENTRY(SIMPLE_WAV_ERR_NOMEM)
};
#undef ERROR_NAME_TABLE_ENTRY

#define RIFF_FILE_INFO_SIZE 8
#define WAV_HEADER_SIZE 44
#define WAV_MAX_SAMPLE_BYTES (((uint64_t)UINT32_MAX) - WAV_HEADER_SIZE + RIFF_FILE_INFO_SIZE)

#define RIFF_FILE_SIZE_OFFSET 4
#define WAV_DATA_BLOC_SIZE_OFFSET 40

struct simple_wav_writer {
    simple_wav_format_info info;
    uint32_t sample_bytes;
    FILE *file;
};

static unsigned char *buffer_write_fourcc(unsigned char *it, const char *s);

static unsigned char *buffer_write_u32_le(unsigned char *it, uint32_t value);

static unsigned char *buffer_write_u16_le(unsigned char *it, uint16_t value);

simple_wav_writer *simple_wav_wr_open(const simple_wav_format_info *info,
                                        const char *path, simple_wav_error *outError) {
    simple_wav_error error = SIMPLE_WAV_SUCCESS;
    simple_wav_writer *wavFile = nullptr;

    if (info->audio_format != SIMPLE_WAV_AUDIO_FMT_PCM || info->bits_per_sample != 16) {
        error = SIMPLE_WAV_ERR_UNSUPPORTED;
        goto finished;
    }

    if (info->sample_rate == 0 || info->sample_rate > UINT32_MAX || info->channel_count == 0 || info->channel_count >
        UINT16_MAX) {
        error = SIMPLE_WAV_ERR_INVALID_ARG;
        goto finished;
    }

    const auto file = fopen(path, "w");
    if (!file) {
        error = SIMPLE_WAV_ERR_FILE_ERROR;
        goto finished;
    }
    const uint16_t bytesPerBloc = (info->channel_count * info->bits_per_sample) >> 3;
    const uint32_t bytesPerSec = bytesPerBloc * info->sample_rate;

    setvbuf(file, nullptr, _IOFBF, 512 * 1024);
    uint8_t wavHeader[WAV_HEADER_SIZE];
    uint8_t *it = wavHeader;

    // RIFF chunk
    it = buffer_write_fourcc(it, "RIFF");
    it = buffer_write_u32_le(it, 0);
    it = buffer_write_fourcc(it, "WAVE");
    // Format chunk
    it = buffer_write_fourcc(it, "fmt ");
    it = buffer_write_u32_le(it, 16);
    it = buffer_write_u16_le(it, info->audio_format);
    it = buffer_write_u16_le(it, info->channel_count);
    it = buffer_write_u32_le(it, info->sample_rate);
    it = buffer_write_u32_le(it, bytesPerSec);
    it = buffer_write_u16_le(it, bytesPerBloc);
    it = buffer_write_u16_le(it, info->bits_per_sample);
    // Data chunk
    it = buffer_write_fourcc(it, "data");
    it = buffer_write_u32_le(it, 0);

    if (fwrite(wavHeader, WAV_HEADER_SIZE, 1, file) != 1) {
        error = SIMPLE_WAV_ERR_FILE_ERROR;
        goto remove_file;
    }

    wavFile = malloc(sizeof(simple_wav_writer));
    if (!wavFile) {
        error = SIMPLE_WAV_ERR_NOMEM;
        goto remove_file;
    }
    wavFile->file = file;
    wavFile->sample_bytes = 0;
    wavFile->info = *info;
    goto finished;

remove_file:
    fclose(file);
    remove(path);
finished:
    if (outError)
        *outError = error;

    return wavFile;
}

void simple_wav_wr_close(simple_wav_writer *w) {
    simple_wav_wr_flush(w);
    fclose(w->file);
    free(w);
}

unsigned simple_wav_wr_write_sample_bytes(simple_wav_writer *w, const void *sampleBuffer, unsigned sampleByteCount,
                                          simple_wav_error *outError) {
    simple_wav_error error = SIMPLE_WAV_SUCCESS;
    const auto totalSampleBytes = w->sample_bytes + sampleByteCount;
    if (totalSampleBytes > WAV_MAX_SAMPLE_BYTES) {
        sampleByteCount = WAV_MAX_SAMPLE_BYTES - w->sample_bytes;
        error = SIMPLE_WAV_ERR_EOF;
    }
    const auto count = fwrite(sampleBuffer, 1, sampleByteCount, w->file);
    if (count == 0 && ferror(w->file)) {
        if (errno == EFBIG)
            error = SIMPLE_WAV_ERR_EOF;
        else
            error = SIMPLE_WAV_ERR_FILE_ERROR;
    }
    w->sample_bytes += count;
    if (outError)
        *outError = error;
    return count;
}

simple_wav_error simple_wav_wr_flush(simple_wav_writer *w) {
    auto error = SIMPLE_WAV_SUCCESS;
    const auto pos = ftell(w->file);
    auto res = fseek(w->file, RIFF_FILE_SIZE_OFFSET, SEEK_SET);
    assert(res != -1);

    uint8_t buffer[4];
    buffer_write_u32_le(buffer, w->sample_bytes + WAV_HEADER_SIZE - RIFF_FILE_INFO_SIZE);

    if (fwrite(buffer, sizeof(buffer), 1, w->file) != 1) {
        error = SIMPLE_WAV_ERR_FILE_ERROR;
        goto finished;
    }
    res = fseek(w->file, WAV_DATA_BLOC_SIZE_OFFSET, SEEK_SET);
    assert(res != -1);

    buffer_write_u32_le(buffer, w->sample_bytes);
    if (fwrite(buffer, sizeof(buffer), 1, w->file) != 1) {
        error = SIMPLE_WAV_ERR_FILE_ERROR;
        goto finished;
    }
    fflush(w->file);
finished:
    res = fseek(w->file, pos, SEEK_SET);
    assert(res != -1);

    assert(res != -1);
    return error;
}

const simple_wav_format_info *simple_wav_wr_get_format_info(const simple_wav_writer *w) {
    return &w->info;
}

unsigned simple_wav_wr_get_sample_bytes_written(const simple_wav_writer *file) {
    return file->sample_bytes;
}


struct simple_wav_reader {
    simple_wav_format_info info;
    uint32_t sample_bytes;
    FILE *file;
};

static unsigned char *buffer_read_fourcc(unsigned char *buf, char (*value)[4]);

static unsigned char *buffer_read_u32_le(unsigned char *it, uint32_t *value);

static unsigned char *buffer_read_u16_le(unsigned char *it, uint16_t *value);

simple_wav_reader *simple_wav_rd_open(const char *path, simple_wav_error *outError) {
    simple_wav_reader *reader = nullptr;
    simple_wav_error error = SIMPLE_WAV_SUCCESS;
    FILE *file = fopen(path, "r");
    if (!file) {
        error = SIMPLE_WAV_ERR_FILE_ERROR;
        goto finished;
    }
    setvbuf(file, nullptr, _IOFBF, 512 * 1024);

    int64_t remainingBytes = 0;
    {
        uint8_t riffHeader[12];
        if (fread(riffHeader, sizeof(riffHeader), 1, file) != 1) {
            error = SIMPLE_WAV_ERR_FILE_ERROR;
            goto close_file;
        }
        auto it = riffHeader;
        char riffMagic[4];
        char wavMagic[4];
        uint32_t size;
        it = buffer_read_fourcc(it, &riffMagic);
        it = buffer_read_u32_le(it, &size);
        it = buffer_read_fourcc(it, &wavMagic);

        if (memcmp(riffMagic, "RIFF", 4) != 0 || size < 28 || memcmp(wavMagic, "WAVE", 4) != 0) {
            error = SIMPLE_WAV_ERR_INVALID_HEADER;
            goto close_file;
        }
        remainingBytes = size;
    }
    // WAV Magic
    remainingBytes -= 4;


    uint8_t fmtChunk[24];
    if (fread(fmtChunk, sizeof(fmtChunk), 1, file) != 1) {
        error = SIMPLE_WAV_ERR_FILE_ERROR;
        goto close_file;
    }
    char fmtMagic[4];
    auto it = buffer_read_fourcc(fmtChunk, &fmtMagic);
    if (memcmp(fmtMagic, "fmt ", 4) != 0) {
        error = SIMPLE_WAV_ERR_INVALID_HEADER;
        goto close_file;
    }
    uint32_t fmtChunkSize;
    uint16_t audioFormat = 0;
    it = buffer_read_u32_le(it, &fmtChunkSize);
    it = buffer_read_u16_le(it, &audioFormat);
    if ((simple_wav_audio_format) audioFormat != SIMPLE_WAV_AUDIO_FMT_PCM) {
        error = SIMPLE_WAV_ERR_UNSUPPORTED;
        goto close_file;
    }
    if (fmtChunkSize != 16) {
        error = SIMPLE_WAV_ERR_INVALID_HEADER;
        goto close_file;
    }
    uint16_t channelCount;
    uint32_t sampleRate;
    uint32_t bytesPerSec;
    uint16_t bytesPerBloc;
    uint16_t bitsPerSample;
    it = buffer_read_u16_le(it, &channelCount);
    it = buffer_read_u32_le(it, &sampleRate);
    it = buffer_read_u32_le(it, &bytesPerSec);
    it = buffer_read_u16_le(it, &bytesPerBloc);
    it = buffer_read_u16_le(it, &bitsPerSample);
    if (((bytesPerSec / sampleRate) != bytesPerBloc) || (bytesPerBloc * 8) != (bitsPerSample * channelCount)) {
        error = SIMPLE_WAV_ERR_INVALID_HEADER;
        goto close_file;
    }
    // FMT Chunk
    remainingBytes -= 24;

    uint8_t chunkHeader[8];

    uint32_t sampleBytes = 0;

    while (remainingBytes > 0) {
        if (fread(chunkHeader, sizeof(chunkHeader), 1, file) != 1) {
            error = SIMPLE_WAV_ERR_FILE_ERROR;
            goto close_file;
        }

        remainingBytes -= sizeof(chunkHeader);
        char blocId[4];
        uint32_t chunkSize;

        it = buffer_read_fourcc(chunkHeader, &blocId);
        it = buffer_read_u32_le(it, &chunkSize);

        if (memcmp(blocId, "data", sizeof(blocId)) != 0) {
            fseek(file, chunkSize, SEEK_CUR);
            remainingBytes -= chunkSize;
            continue;
        }
        sampleBytes = chunkSize;
        break;
    }

    if (sampleBytes == 0) {
        goto close_file;
    }

    reader = (simple_wav_reader *) malloc(sizeof(simple_wav_reader));
    if (!reader) {
        error = SIMPLE_WAV_ERR_NOMEM;
        goto close_file;
    }
    reader->info.sample_rate = sampleRate;
    reader->info.bits_per_sample = bitsPerSample;
    reader->info.audio_format = (simple_wav_audio_format) audioFormat;
    reader->info.channel_count = channelCount;
    reader->file = file;
    reader->sample_bytes = remainingBytes;
    goto finished;
close_file:
    fclose(file);

finished:
    if (outError)
        *outError = error;
    return reader;
}

void simple_wav_rd_close(simple_wav_writer *w) {
    fclose(w->file);
    free(w);
}

const simple_wav_format_info *simple_wav_rd_get_format_info(const simple_wav_reader *w) {
    return &w->info;
}

unsigned simple_wav_rd_get_sample_bytes_remaining(const simple_wav_reader *file) {
    return file->sample_bytes;
}

unsigned simple_wav_rd_read_sample_bytes(simple_wav_reader *r, void *sampleBuffer, unsigned sampleByteCount,
                                         simple_wav_error *outError) {
    auto error = SIMPLE_WAV_SUCCESS;
    size_t bytesRead = 0;

    if (r->sample_bytes == 0) {
        error = SIMPLE_WAV_ERR_EOF;
        goto finished;
    } else if (sampleByteCount > r->sample_bytes)
        sampleByteCount = r->sample_bytes;

    bytesRead = fread(sampleBuffer, 1, sampleByteCount, r->file);

    r->sample_bytes -= bytesRead;

    if (bytesRead == 0) {
        if (feof(r->file))
            error = SIMPLE_WAV_ERR_EOF;
        else
            error = SIMPLE_WAV_ERR_FILE_ERROR;
    }
finished:
    if (outError)
        *outError = error;
    return bytesRead;
}

static unsigned char *buffer_write_fourcc(unsigned char *it, const char *s) {
    it[0] = s[0];
    it[1] = s[1];
    it[2] = s[2];
    it[3] = s[3];
    return it + 4;
}

static unsigned char *buffer_write_u32_le(unsigned char *it, uint32_t value) {
    it[0] = value >> 0;
    it[1] = value >> 8;
    it[2] = value >> 16;
    it[3] = value >> 24;
    return it + sizeof(value);
}

static unsigned char *buffer_write_u16_le(unsigned char *it, uint16_t value) {
    it[0] = value >> 0;
    it[1] = value >> 8;
    return it + sizeof(value);
}

static unsigned char *buffer_read_fourcc(unsigned char *buf, char (*value)[4]) {
    (*value)[0] = (char) buf[0];
    (*value)[1] = (char) buf[1];
    (*value)[2] = (char) buf[2];
    (*value)[3] = (char) buf[3];
    return buf + sizeof(*value);
}

static unsigned char *buffer_read_u32_le(unsigned char *it, uint32_t *value) {
    *value = ((uint32_t) it[0]) | ((uint32_t) it[1] << 8) | ((uint32_t) it[2] << 16) | ((uint32_t) it[3] << 24);
    return it + sizeof(*value);
}

static unsigned char *buffer_read_u16_le(unsigned char *it, uint16_t *value) {
    *value = it[0] | ((uint16_t) it[1] << 8);
    return it + sizeof(*value);
}

const char *simple_wav_strerrorname(simple_wav_error error) {
    if (error > sizeof(ERROR_NAME_TABLE))
        return "SIMPLE_WAV_UNKNOWN_ERROR";
    return ERROR_NAME_TABLE[error];
}
