#pragma once
#include <wut.h>
#include <coreinit/event.h>

#ifdef __cplusplus
extern "C" {
#endif

#define UAC_OPT_READ_FLAG 0x80

typedef struct UACIpcWorkMemory UACIpcWorkMemory;
typedef struct UACRequestData UACRequestData;
typedef struct UACSampleBufInfo UACSampleBufInfo;
typedef struct UACISODesc UACISODesc;

typedef enum UACError {
    UAC_ERROR_IPC_POOL_UNINITIALIZED = -1638452,
    UAC_ERROR_IPC_ALLOC_FAILED = -1638451,
    UAC_ERROR_INVALID_ISO_DESC_IPC_BUFFER_SIZE = -1638438,
    UAC_ERROR_INVALID_IPC_BUFFER_SIZE = -1638437,
    UAC_ERROR_IOS_OPEN_FAILED = -1638435,
    UAC_ERROR_IOCTL_FAILED = -1638434,
    UAC_ERROR_NOT_OPEN = -1638433,
    UAC_ERROR_ALREADY_CALLED = -1638432,
    UAC_ERROR_UNINITIALIZED = -1638431,
    UAC_ERROR_INVALID_ARG = -1638430,
    UAC_SUCCESS = 0
} UACError;
WUT_CHECK_SIZE(UACError, 4);

typedef enum UACChannel {
    UAC_CHANNEL_0,
    UAC_CHANNEL_1
} UACChannel;
WUT_CHECK_SIZE(UACChannel, 4);


struct WUT_PACKED UACRequestData {
    uint8_t id;
    uint8_t opt;
    //! Always set to 1 by mic.rpl
    uint16_t unk;
    void *buffer;
    uint32_t size;
    //! Size set after a request with UAC_OPT_READ_FLAG set on opt
    uint32_t returnedSize;
};

WUT_CHECK_OFFSET(UACRequestData, 0x00, id);
WUT_CHECK_OFFSET(UACRequestData, 0x01, opt);
WUT_CHECK_OFFSET(UACRequestData, 0x02, unk);
WUT_CHECK_OFFSET(UACRequestData, 0x04, buffer);
WUT_CHECK_OFFSET(UACRequestData, 0x08, size);
WUT_CHECK_OFFSET(UACRequestData, 0x0c, returnedSize);
WUT_CHECK_SIZE(UACRequestData, 0x10);


//! Both buffers must be allocated from an IPCBufPool
struct UACIpcWorkMemory {
    void *buffer;
    // Must be a multiple of 2048
    uint32_t bufferSizeBytes;
    void *isoDescBuffer;
    // Must equal (bufferSizeInBytes * 96) / 2048
    uint32_t isoDescBufferSizeBytes;
};

WUT_CHECK_OFFSET(UACIpcWorkMemory, 0x00, buffer);
WUT_CHECK_OFFSET(UACIpcWorkMemory, 0x04, bufferSizeBytes);
WUT_CHECK_OFFSET(UACIpcWorkMemory, 0x08, isoDescBuffer);
WUT_CHECK_OFFSET(UACIpcWorkMemory, 0x0c, isoDescBufferSizeBytes);
WUT_CHECK_SIZE(UACIpcWorkMemory, 0x10);

struct UACSampleBufInfo {
    //! Not sure of this, here because of logging in mic.rpl
    uint16_t frameSlipMs;
    uint16_t sizeBytes;
};
WUT_CHECK_OFFSET(UACSampleBufInfo, 0x00, frameSlipMs);
WUT_CHECK_OFFSET(UACSampleBufInfo, 0x02, sizeBytes);
WUT_CHECK_SIZE(UACSampleBufInfo, 0x04);

struct UACISODesc {
    //! May just be padding, seem to always be zero
    WUT_UNKNOWN_BYTES(0x20);
    void* sampleBufs[0x08];
    UACSampleBufInfo bufInfo[0x08];
};
WUT_CHECK_OFFSET(UACISODesc, 0x20, sampleBufs);
WUT_CHECK_OFFSET(UACISODesc, 0x40, bufInfo);
WUT_CHECK_SIZE(UACISODesc, 0x60);

/**
 * Initialize the UAC library
 * \return UAC_SUCCESS - success
 * \return UAC_ERROR_ALREADY_CALLED - this function was previously called
 * \return UAC_ERROR_IPC_ALLOC_FAILED - failed to initialize IPC buf pool
 * \note This function can only be called once per process, as the init called flag is still set on failure
 */
UACError UACInit();

/**
 * Open the GamePad microphone
 * \param channel target GamePad
 * \param workMem memory to be used for capture
 * \return UAC_SUCCESS - success
 * \return UAC_ERROR_INVALID_ARG - invalid channel, workMem is NULL or workMem buffers are NULL
 * \return UAC_ERROR_IPC_BUFFER_SIZE - first buffer size is not a multiple of 2048
 * \return UAC_ERROR_ISO_DESC_IPC_BUFFER_SIZE - second buffer size is not (firstBufferSize * 96) / 2048
 * \return UAC_ERROR_ALREADY_CALLED - called when already open
 * \return UAC_ERROR_UNINITIALIZED - UAC library is not initialized (UACInit)
 * \return UAC_ERROR_IOS_OPEN_FAILED - failed to open /dev/ccr_uac
 * \return UAC_ERROR_IPC_POOL_UNINITIALIZED - IPC pool is not initialized (UACInit)
 * \return UAC_ERROR_IPC_ALLOC_FAILED - failed to allocate message for IPC call
 * \return UAC_ERROR_IOCTL_FAILED - error occurred during IPC
 */
UACError UACOpen(UACChannel channel,
                 const UACIpcWorkMemory *workMem);

/**
 * Close the GamePad microphone
 * \param channel target GamePad
 * \return UAC_SUCESS - success
 * \return UAC_INVALID_ARG - channel is not valid
 * \return UAC_ERROR_UNINITIALIZED - UAC library is not initialized (UACInit)
 * \return UAC_ERROR_NOT_OPEN - mic is not open
 * \return UAC_ERROR_IPC_POOL_UNINITIALIZED - IPC pool is not initialized (UACInit)
 * \return UAC_ERROR_IPC_ALLOC_FAILED - failed to allocate message for IPC call
 * \return UAC_ERROR_IOCTL_FAILED - error occurred during IPC
 */
UACError UACClose(UACChannel channel);

/**
 * Get 16-bit little-endian PCM samples at 16kHz sample rate
 * \param channel target GamePad
 * \param event signalled when audio is retrieved
 * \param [out] outDesc pointer to desc to be allocated by UACGetAudio
 * \return UAC_SUCCESS - success
 * \return UAC_INVALID_ARG - channel is not valid, event is NULL or outDesc is NULL
 * \return UAC_ERROR_UNINITIALIZED - UAC library is not initialized (UACInit)
 * \return UAC_ERROR_NOT_OPEN - mic is not open
 * \return UAC_ERROR_IPC_POOL_UNINITIALIZED - IPC pool is not initialized (UACInit)
 * \return UAC_ERROR_IPC_ALLOC_FAILED - failed to allocate message for IPC call
 * \return UAC_ERROR_IOCTL_FAILED - error occurred during IPC
 * \note confounding as the mic library (which depends on this) has a sample rate of 32kHz
 */
UACError UACGetAudio(UACChannel channel,
                     OSEvent* event,
                     UACISODesc** outDesc);

/**
 * Free ISODesc that was received via UACGetAudio
 * \param channel GamePad which provided the ISODesc
 * \param desc isoDesc previously acquired
 * \return UAC_SUCCESS - success
 * \return UAC_INVALID_ARG - desc was not provided by this channel
 * \return UAC_ERROR_NOT_OPEN - mic is not open
 * \return UAC_ERROR_IPC_POOL_UNINITIALIZED - IPC pool is not initialized (UACInit)
 * \return UAC_ERROR_IPC_ALLOC_FAILED - failed to allocate message for IPC call
 * \return UAC_ERROR_IOCTL_FAILED - error occurred during IPC
 */
UACError UACFreeISODesc(UACChannel channel,
                        UACISODesc* desc);

UACError UACRequest(UACChannel channel,
    UACRequestData* request);

#ifdef __cplusplus
}
#endif
