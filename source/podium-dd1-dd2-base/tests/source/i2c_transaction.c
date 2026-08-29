#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

#include "i2c/transaction.h"

typedef struct {
    uint8_t submit_result;
    uint8_t poll_result;
    uint8_t submissions;
    uint8_t polls;
    uint8_t recoveries;
    I2cTransactionRequest request;
} DriverState;

static uint8_t submit(void *context, I2cTransactionRequest request) {
    DriverState *state = context;
    state->submissions++;
    state->request = request;
    return state->submit_result;
}

static uint8_t poll(void *context) {
    DriverState *state = context;
    state->polls++;
    return state->poll_result;
}

static void recover(void *context) {
    DriverState *state = context;
    state->recoveries++;
}

static I2cTransactionDriver driver(DriverState *state) {
    return (I2cTransactionDriver){
        .context = state,
        .submit = submit,
        .poll = poll,
        .recover = recover,
    };
}

static void test_submits_then_completes_an_accepted_request(void) {
    DriverState state = {0};
    I2cTransactionDriver operations = driver(&state);
    I2cTransaction transaction;
    i2c_transaction_init(&transaction);
    uint8_t data[3] = {0};
    I2cTransactionRequest request = {
        .channel = 0xf0,
        .data = data,
        .length = sizeof(data),
        .parameter = 0x12345678,
        .read = true,
    };

    assert(i2c_transaction_service(&transaction, &operations, request) == I2C_TRANSACTION_PENDING);
    assert(transaction.state == I2C_TRANSACTION_POLLING);
    assert(state.submissions == 1);
    assert(state.request.channel == 0xf0);
    assert(state.request.data == data);
    assert(state.request.length == sizeof(data));
    assert(state.request.parameter == 0x12345678);
    assert(state.request.read);

    assert(i2c_transaction_service(&transaction, &operations, request) == I2C_TRANSACTION_COMPLETE);
    assert(transaction.state == I2C_TRANSACTION_IDLE);
    assert(state.polls == 1);
}

static void test_pending_poll_status_keeps_polling(void) {
    DriverState state = {.poll_result = 1};
    I2cTransactionDriver operations = driver(&state);
    I2cTransaction transaction;
    i2c_transaction_init(&transaction);
    I2cTransactionRequest request = {0};

    i2c_transaction_service(&transaction, &operations, request);
    assert(i2c_transaction_service(&transaction, &operations, request) == I2C_TRANSACTION_PENDING);
    assert(transaction.state == I2C_TRANSACTION_POLLING);
    assert(state.polls == 1);
}

static void test_poll_errors_recover_on_the_following_service(void) {
    const uint8_t errors[] = {2, 4};

    for (uint8_t index = 0; index < sizeof(errors); index++) {
        DriverState state = {.poll_result = errors[index]};
        I2cTransactionDriver operations = driver(&state);
        I2cTransaction transaction;
        i2c_transaction_init(&transaction);
        I2cTransactionRequest request = {0};

        i2c_transaction_service(&transaction, &operations, request);
        assert(i2c_transaction_service(&transaction, &operations, request) ==
               I2C_TRANSACTION_PENDING);
        assert(transaction.state == I2C_TRANSACTION_RECOVERY);
        assert(i2c_transaction_service(&transaction, &operations, request) ==
               I2C_TRANSACTION_RECOVERED);
        assert(transaction.state == I2C_TRANSACTION_IDLE);
        assert(state.recoveries == 1);
    }
}

static void test_submission_failure_uses_the_same_recovery_path(void) {
    DriverState state = {.submit_result = 1};
    I2cTransactionDriver operations = driver(&state);
    I2cTransaction transaction;
    i2c_transaction_init(&transaction);
    I2cTransactionRequest request = {0};

    assert(i2c_transaction_service(&transaction, &operations, request) == I2C_TRANSACTION_PENDING);
    assert(transaction.state == I2C_TRANSACTION_RECOVERY);
    assert(i2c_transaction_service(&transaction, &operations, request) ==
           I2C_TRANSACTION_RECOVERED);
    assert(state.recoveries == 1);
}

int main(void) {
    test_submits_then_completes_an_accepted_request();
    test_pending_poll_status_keeps_polling();
    test_poll_errors_recover_on_the_following_service();
    test_submission_failure_uses_the_same_recovery_path();
    return 0;
}
