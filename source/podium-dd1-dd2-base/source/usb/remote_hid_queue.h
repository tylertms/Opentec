#ifndef OPENTEC_BASE_USB_REMOTE_HID_QUEUE_H
#define OPENTEC_BASE_USB_REMOTE_HID_QUEUE_H

#include <stdbool.h>
#include <stdint.h>

#if defined(__GNUC__)
#define USB_REMOTE_HID_PACKED __attribute__((packed))
#else
#define USB_REMOTE_HID_PACKED
#endif

/** @brief Native remote-HID queue dimensions and report framing. */
enum {
    USB_REMOTE_HID_QUEUE_CAPACITY = 32,       /**< Number of sparse queue records. */
    USB_REMOTE_HID_QUEUE_RECORD_SIZE = 5,     /**< Serialized size of one queue record. */
    USB_REMOTE_HID_REPORT_SIZE = 64,          /**< Complete feature report size. */
    USB_REMOTE_HID_REPORT_HEADER_SIZE = 3,    /**< Marker, report identifier, and type bytes. */
    USB_REMOTE_HID_REPORT_PAYLOAD_SIZE = 61,  /**< Bytes available after the feature header. */
    USB_REMOTE_HID_REPORT_RECORD_LIMIT = 12,  /**< Complete records fitting in one report. */
    USB_REMOTE_HID_REPORT_ID = 5,             /**< Remote-HID report identifier. */
    USB_REMOTE_HID_REPORT_TYPE = 1,           /**< Remote-HID report type. */
    USB_REMOTE_HID_MARKER_NATIVE = 0xff,      /**< Native Fanatec marker. */
    USB_REMOTE_HID_MARKER_PLAYSTATION = 0x35, /**< PlayStation marker. */
    USB_REMOTE_HID_MARKER_XBOX = 0x36,        /**< Xbox marker. */
};

/** @brief Host transport used to select the remote-HID report marker. */
typedef enum {
    USB_REMOTE_HID_HOST_NATIVE,      /**< Native Fanatec transport. */
    USB_REMOTE_HID_HOST_PLAYSTATION, /**< PlayStation transport. */
    USB_REMOTE_HID_HOST_XBOX,        /**< Xbox transport. */
} UsbRemoteHidHost;

/** @brief One packed five-byte official remote-HID queue record. */
typedef struct USB_REMOTE_HID_PACKED {
    uint16_t flags;     /**< Record type, index, and overlay flags. */
    uint8_t value_low;  /**< Low byte of the record value. */
    uint8_t value_high; /**< High byte of the record value. */
    uint8_t data;       /**< Record data byte. */
} UsbRemoteHidQueueRecord;

/** @brief Sparse queue matching the official thirty-two-record storage. */
typedef struct {
    UsbRemoteHidQueueRecord records[USB_REMOTE_HID_QUEUE_CAPACITY]; /**< Queue slots. */
} UsbRemoteHidQueue;

typedef char usb_remote_hid_queue_record_size_check
    [(sizeof(UsbRemoteHidQueueRecord) == USB_REMOTE_HID_QUEUE_RECORD_SIZE) ? 1 : -1];
typedef char usb_remote_hid_queue_size_check[(sizeof(UsbRemoteHidQueue) == 160) ? 1 : -1];

/**
 * @brief Initializes a remote-HID queue.
 *
 * Clears every sparse queue slot.
 *
 * @param[out] queue Queue to initialize.
 */
void usb_remote_hid_queue_init(UsbRemoteHidQueue *queue);

/**
 * @brief Enqueues one remote-HID record.
 *
 * Stores the record in the first empty sparse slot. A zero flags value is not a valid official
 * queue record and is rejected.
 *
 * @param[in,out] queue Queue receiving the record.
 * @param[in] record Record to copy.
 * @return True when a slot was available and the record was stored.
 */
bool usb_remote_hid_queue_enqueue(UsbRemoteHidQueue *queue, const UsbRemoteHidQueueRecord *record);

/**
 * @brief Reports whether a queue contains a record.
 *
 * @param[in] queue Queue to inspect.
 * @return True when at least one record has nonzero flags.
 */
bool usb_remote_hid_queue_pending(const UsbRemoteHidQueue *queue);

/**
 * @brief Encodes and drains one official remote-HID feature report.
 *
 * The output is always cleared first. At most twelve five-byte records are copied in sparse-slot
 * order, and copied records are cleared. An empty queue returns false and leaves a zero-filled
 * report; a nonempty queue writes the host marker, report identifier, and type.
 *
 * @param[in,out] queue Queue to drain.
 * @param[in] host Host transport selecting the report marker.
 * @param[out] report Complete 64-byte report destination.
 * @return True when at least one record was encoded; otherwise false.
 */
bool usb_remote_hid_queue_encode(UsbRemoteHidQueue *queue, UsbRemoteHidHost host,
                                 uint8_t report[USB_REMOTE_HID_REPORT_SIZE]);

#undef USB_REMOTE_HID_PACKED

#endif
