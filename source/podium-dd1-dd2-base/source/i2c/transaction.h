#ifndef OPENTEC_BASE_I2C_TRANSACTION_H
#define OPENTEC_BASE_I2C_TRANSACTION_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    I2C_TRANSACTION_IDLE,
    I2C_TRANSACTION_POLLING,
    I2C_TRANSACTION_RECOVERY,
} I2cTransactionState;

typedef enum {
    I2C_TRANSACTION_PENDING,
    I2C_TRANSACTION_COMPLETE,
    I2C_TRANSACTION_RECOVERED,
} I2cTransactionResult;

typedef struct {
    uint8_t channel;
    uint8_t *data;
    uint16_t length;
    uint32_t parameter;
    bool read;
} I2cTransactionRequest;

typedef uint8_t (*I2cTransactionSubmit)(void *context, I2cTransactionRequest request);
typedef uint8_t (*I2cTransactionPoll)(void *context);
typedef void (*I2cTransactionRecover)(void *context);

typedef struct {
    void *context;
    I2cTransactionSubmit submit;
    I2cTransactionPoll poll;
    I2cTransactionRecover recover;
} I2cTransactionDriver;

typedef struct {
    I2cTransactionState state;
} I2cTransaction;

/**
 * @brief Initializes an asynchronous I2C transaction channel.
 *
 * Starts the channel in its submission state with no transfer pending.
 *
 * @param[out] transaction Transaction channel to initialize.
 */
void i2c_transaction_init(I2cTransaction *transaction);

/**
 * @brief Advances one asynchronous I2C parameter transfer.
 *
 * Submits an idle request, polls an accepted request, and recovers the controller after a
 * rejected submission or transfer error.
 *
 * @param[in,out] transaction Persistent transaction-channel state.
 * @param[in] driver Transfer submission, completion-poll, and recovery operations.
 * @param[in] request Channel, local data, length, parameter offset, and transfer direction.
 * @return Pending, complete, or recovered transaction status.
 */
I2cTransactionResult i2c_transaction_service(I2cTransaction *transaction,
                                             const I2cTransactionDriver *driver,
                                             I2cTransactionRequest request);

#endif
