#include "i2c/transaction.h"

#include <stdint.h>

enum {
    I2C_DRIVER_COMPLETE = 0,
    I2C_DRIVER_ERROR = 2,
    I2C_DRIVER_ALTERNATE_ERROR = 4,
};

/**
 * @brief Initializes an asynchronous I2C transaction channel.
 *
 * Starts the channel in its submission state with no transaction pending.
 *
 * @param[out] transaction Transaction channel to initialize.
 */
void i2c_transaction_init(I2cTransaction *transaction) {
    transaction->state = I2C_TRANSACTION_IDLE;
}

/**
 * @brief Advances one asynchronous I2C transaction.
 *
 * Submits an idle request and polls an accepted request on later calls. Poll status 0 completes
 * the request, while status 2 or 4 schedules controller recovery. Recovery reinitializes the
 * controller on the following call and returns status 2 before the channel becomes idle again.
 *
 * @param[in,out] transaction Persistent transaction-channel state.
 * @param[in] driver I2C submission, completion-poll, and recovery operations.
 * @param[in] request Address, buffer, length, and transfer direction to submit.
 * @return Pending, complete, or recovered transaction status.
 */
I2cTransactionResult i2c_transaction_service(I2cTransaction *transaction,
                                             const I2cTransactionDriver *driver,
                                             I2cTransactionRequest request) {
    if (transaction->state == I2C_TRANSACTION_IDLE) {
        transaction->state = driver->submit(driver->context, request) == 0
                                 ? I2C_TRANSACTION_POLLING
                                 : I2C_TRANSACTION_RECOVERY;
        return I2C_TRANSACTION_PENDING;
    }

    if (transaction->state == I2C_TRANSACTION_RECOVERY) {
        transaction->state = I2C_TRANSACTION_IDLE;
        driver->recover(driver->context);
        return I2C_TRANSACTION_RECOVERED;
    }

    uint8_t result = driver->poll(driver->context);
    if (result == I2C_DRIVER_COMPLETE) {
        transaction->state = I2C_TRANSACTION_IDLE;
        return I2C_TRANSACTION_COMPLETE;
    }
    if (result == I2C_DRIVER_ERROR || result == I2C_DRIVER_ALTERNATE_ERROR) {
        transaction->state = I2C_TRANSACTION_RECOVERY;
    }
    return I2C_TRANSACTION_PENDING;
}
