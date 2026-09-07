#ifndef SIMPLE_WAV_LIBRARY_H
#define SIMPLE_WAV_LIBRARY_H
#include <stdint.h>

#if defined(__clang__)
#define SIMPLE_WAV_ATTR_NONNULL(...) __attribute__((nonnull(__VA_ARGS__)))
#define SIMPLE_WAV_ATTR_NT_STRING(argIndex)
#define SIMPLE_WAV_ATTR_WRITE(argIndex)
#define SIMPLE_WAV_ATTR_READ_SIZED(argIndex, sizeIndex)
#define SIMPLE_WAV_ATTR_WRITE_SIZED(argIndex, sizeIndex)
#define SIMPLE_WAV_RETURNS_NONNULL __attribute__((returns_nonnull))
#define SIMPLE_WAV_NODISCARD __attribute__((warn_unused_result))
#define SIMPLE_WAV_CONST __attribute__((const))
#define SIMPLE_WAV_PURE __attribute__((pure))
#elif defined(__GNUC__)
#define SIMPLE_WAV_ATTR_NONNULL(...) __attribute__((nonnull(__VA_ARGS__)))
#define SIMPLE_WAV_ATTR_NT_STRING(argIndex) __attribute((null_terminated_string_arg(argIndex)))
#define SIMPLE_WAV_ATTR_WRITE(argIndex) __attribute__((access(write_only, argIndex)))
#define SIMPLE_WAV_ATTR_READ_SIZED(argIndex, sizeIndex) __attribute__((access(read_only, argIndex, sizeIndex)))
#define SIMPLE_WAV_ATTR_WRITE_SIZED(argIndex, sizeIndex) __attribute__((access(write_only, argIndex, sizeIndex)))
#define SIMPLE_WAV_RETURNS_NONNULL __attribute__((returns_nonnull))
#define SIMPLE_WAV_NODISCARD __attribute__((warn_unused_result))
#define SIMPLE_WAV_CONST __attribute__((const))
#define SIMPLE_WAV_PURE __attribute__((pure))
#else
#define SIMPLE_WAV_ATTR_NONNULL(...)
#define SIMPLE_WAV_ATTR_NT_STRING(argIndex)
#define SIMPLE_WAV_ATTR_WRITE(argIndex)
#define SIMPLE_WAV_ATTR_READ_SIZED(argIndex, sizeIndex)
#define SIMPLE_WAV_ATTR_WRITE_SIZED(argIndex, sizeIndex)
#define SIMPLE_WAV_RETURNS_NONNULL
#define SIMPLE_WAV_NODISCARD
#define SIMPLE_WAV_CONST
#endif

typedef enum {
    SIMPLE_WAV_SUCCESS = 0,
    SIMPLE_WAV_ERR_FILE_ERROR = 1,
    SIMPLE_WAV_ERR_INVALID_ARG = 2,
    SIMPLE_WAV_ERR_EOF = 3,
    SIMPLE_WAV_ERR_UNSUPPORTED = 4,
    SIMPLE_WAV_ERR_INVALID_HEADER = 5,
    SIMPLE_WAV_ERR_NOMEM = 6,
} simple_wav_error;

typedef enum {
    SIMPLE_WAV_AUDIO_FMT_PCM = 1
} simple_wav_audio_format;

typedef struct {
    unsigned channel_count;
    unsigned sample_rate;
    unsigned bits_per_sample;
    simple_wav_audio_format audio_format;
} simple_wav_format_info;

typedef struct simple_wav_writer simple_wav_writer;
typedef struct simple_wav_reader simple_wav_reader;

#ifdef __cplusplus
extern "C" {
#endif


/**
 * Create a WAV file, and return a handle for writing to it
 * \param info format of the WAV
 * \param path path where the file should be created or path to the file to be overwritten
 * \param [out] outError pointer to return error to
 * \return WAV handle
 *
 * Errors:
 * - SIMPLE_WAV_ERR_UNSUPPORTED - format is unsupported
 * - SIMPLE_WAV_ERR_INVALID_ARG - contents of info are invalid
 * - SIMPLE_WAV_ERR_FILE_ERROR - file could not be created, opened, or written to
 * - SIMPLE_WAV_ERR_NOMEM - not enough memory to allocate writer
 */
simple_wav_writer *simple_wav_wr_open(const simple_wav_format_info *info, const char *path,
                                            simple_wav_error *outError)
SIMPLE_WAV_ATTR_NONNULL(1) SIMPLE_WAV_ATTR_NT_STRING(2) SIMPLE_WAV_ATTR_NONNULL(2) SIMPLE_WAV_ATTR_WRITE(3)
SIMPLE_WAV_NODISCARD;

//! Flush WAV contents and free the resources associated with the WAV and
void simple_wav_wr_close(simple_wav_writer *w)
SIMPLE_WAV_ATTR_NONNULL(1);

/**
 * Write samples to WAV file
 * \param sampleBuffer sample data to write
 * \param sampleByteCount number of bytes of sample data
 * \param [out] outError returned error
 * \return number of bytes actually written.
 *
 * Errors:
 * - SIMPLE_WAV_ERR_EOF - cannot write any more bytes
 * - SIMPLE_WAV_ERR_FILE_ERROR - unspecified file error occurred
 */
unsigned simple_wav_wr_write_sample_bytes(simple_wav_writer *w, const void *sampleBuffer, unsigned sampleByteCount,
                                       simple_wav_error *outError)
SIMPLE_WAV_ATTR_NONNULL(1) SIMPLE_WAV_ATTR_NONNULL(2) SIMPLE_WAV_ATTR_READ_SIZED(2, 3) SIMPLE_WAV_ATTR_WRITE(4);

/**
 * Ensure that the contents of the WAV are written to disk, with the correct sizes
 * \return SIMPLE_WAV_SUCCESS - no errors occurred
 * \return SIMPLE_WAV_ERR_FILE_ERROR - an error occurred while writing the file
 */
simple_wav_error simple_wav_wr_flush(simple_wav_writer *w)
SIMPLE_WAV_ATTR_NONNULL(1);

//! Get WAV format info
const simple_wav_format_info *simple_wav_wr_get_format_info(const simple_wav_writer *w)
SIMPLE_WAV_ATTR_NONNULL(1) SIMPLE_WAV_RETURNS_NONNULL SIMPLE_WAV_NODISCARD;

//! Get number of bytes of sample data written to file
unsigned simple_wav_wr_get_sample_bytes_written(const simple_wav_writer *file)
SIMPLE_WAV_ATTR_NONNULL(1) SIMPLE_WAV_NODISCARD SIMPLE_WAV_PURE;


/**
 * Open a WAV file, and return a handle for reading from it
 * \param path location of the file to read
 * \param [out] outError pointer to return error to
 * \return WAV handle
 *
 * Errors:
 * - SIMPLE_WAV_ERR_UNSUPPORTED - format is unsupported
 * - SIMPLE_WAV_ERR_INVALID_HEADER - header is in an unexpected format
 * - SIMPLE_WAV_ERR_FILE_ERROR - file could not be opened or read from
 * - SIMPLE_WAV_ERR_NOMEM -  not enough memory to allocate reader
 */
simple_wav_reader *simple_wav_rd_open(const char *path, simple_wav_error *outError)
SIMPLE_WAV_ATTR_NONNULL(1) SIMPLE_WAV_ATTR_NT_STRING(1) SIMPLE_WAV_ATTR_WRITE(2) SIMPLE_WAV_NODISCARD;

//! Close the WAV file
void simple_wav_rd_close(simple_wav_writer *w)
SIMPLE_WAV_ATTR_NONNULL(1);

//! Get number of bytes of sample data remaining (updated with each call to simple_wav_rd_read_sample_bytes)
unsigned simple_wav_rd_get_sample_bytes_remaining(const simple_wav_reader *file)
SIMPLE_WAV_ATTR_NONNULL(1) SIMPLE_WAV_NODISCARD SIMPLE_WAV_PURE;

//! Get WAV format info
const simple_wav_format_info *simple_wav_rd_get_format_info(const simple_wav_reader *w)
SIMPLE_WAV_ATTR_NONNULL(1) SIMPLE_WAV_RETURNS_NONNULL SIMPLE_WAV_NODISCARD SIMPLE_WAV_CONST;

/**
 * Write samples to WAV file
 * \param sampleBuffer sample data to write
 * \param sampleByteCount max number of bytes of sample data to read
 * \param [out] outError returned error
 * \return number of bytes actually read
 *
 * Errors:
 * - SIMPLE_WAV_ERR_EOF - cannot write any more bytes
 * - SIMPLE_WAV_ERR_FILE_ERROR - unspecified file error occurred
 */
unsigned simple_wav_rd_read_sample_bytes(simple_wav_reader *r, void *sampleBuffer, unsigned sampleByteCount,
                                             simple_wav_error *outError)
SIMPLE_WAV_ATTR_NONNULL(1) SIMPLE_WAV_ATTR_NONNULL(2) SIMPLE_WAV_ATTR_WRITE_SIZED(2, 3) SIMPLE_WAV_ATTR_WRITE(4);

//! Error enum to string
const char *simple_wav_strerrorname(simple_wav_error error)
SIMPLE_WAV_RETURNS_NONNULL SIMPLE_WAV_NODISCARD SIMPLE_WAV_CONST;
#ifdef __cplusplus
}
#endif
#endif // SIMPLE_WAV_LIBRARY_H
