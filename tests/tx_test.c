#include "test_common.h"
#include <string.h>
#include "api_tests.h"
#include "transaction_file.h"
#include "api.h"
#include "aux.h"
#include "iota/conversion.h"
// include the c-file to be able to test static functions
#include "bundle_ext.c"

void seed_derive_from_bip32(const unsigned int *path, unsigned int pathLength,
                            unsigned char *seed_bytes)
{
    UNUSED(path);
    UNUSED(pathLength);

    chars_to_bytes(mock_ptr_type(char *), seed_bytes, NUM_HASH_TRYTES);
}

void io_send(const void *ptr, unsigned int length, unsigned short sw)
{
    check_expected(ptr);
    check_expected(length);
    check_expected(sw);
}

static void test_bundle(char *seed, TX_INPUT *txs, char *bundle_hash,
                        char signatures[][SIGNATURE_LENGTH])
{
    UNUSED(signatures);
    static const int security = 2;

    api_initialize();
    EXPECT_API_SET_SEED_OK(seed, security);
    EXPECT_API_SET_BUNDLE_OK(txs, 5, bundle_hash);
}

static void test_bundles_for_seed_from_file(void **state)
{
    UNUSED(state);

    test_for_each_bundle("generateBundlesForSeed", test_bundle);
}

static void construct_bundle_from_input(TX_INPUT *input, unsigned int num_txs,
                                        BUNDLE_CTX *bundle_ctx)
{
    TX_ENTRY txs[num_txs];
    for (unsigned int i = 0; i < num_txs; i++) {
        rpad_chars(txs[i].address, input[i].address, NUM_HASH_TRYTES);
        rpad_chars(txs[i].tag, input[i].tag, 27);
        txs[i].value = input[i].value;
        txs[i].timestamp = input[i].timestamp;
    }

    bundle_create(txs, num_txs, bundle_ctx);
}

static void finalize_bundle(TX_INPUT *input, unsigned int last_index)
{
    BUNDLE_CTX bundle;
    construct_bundle_from_input(input, last_index + 1, &bundle);

    uint32_t tag_increment = bundle_finalize(&bundle);

    char extended_tag[NUM_HASH_TRYTES];
    unsigned char tag_bytes[NUM_HASH_BYTES];
    rpad_chars(extended_tag, input[0].tag, NUM_HASH_TRYTES);
    chars_to_bytes(extended_tag, tag_bytes, NUM_HASH_TRYTES);

    bytes_add_u32_mem(tag_bytes, tag_increment);
    bytes_to_chars(tag_bytes, extended_tag, NUM_HASH_BYTES);

    memcpy(input[0].tag, extended_tag, 27);

    // update indices
    for (unsigned int i = 0; i <= last_index; i++) {
        input[i].current_index = i;
        input[i].last_index = last_index;
    }
}

/** Test that the bundle finalization above is correct. */
static void test_refinalize_valid_bundle(void **state)
{
    UNUSED(state);
    static const int security = 2;

    TX_INPUT txs[8];
    // output transaction
    memcpy(&txs[0], &PETER_VECTOR.bundle[0], sizeof(TX_INPUT));
    txs[0].tag[0] = '\0';
    // input transaction
    memcpy(&txs[1], &PETER_VECTOR.bundle[1], sizeof(TX_INPUT));
    // meta transaction
    memcpy(&txs[2], &PETER_VECTOR.bundle[2], sizeof(TX_INPUT));

    // create a valid bundle
    finalize_bundle(txs, 2);

    api_initialize();
    EXPECT_API_SET_SEED_OK(PETER_VECTOR.seed, security);
    EXPECT_API_SET_BUNDLE_OK(txs, 2, PETER_VECTOR.bundle_hash);
}

static void test_payment_higher_than_balance(void **state)
{
    UNUSED(state);
    static const int security = 2;

    TX_INPUT txs[8];
    // output transaction
    memcpy(&txs[0], &PETER_VECTOR.bundle[0], sizeof(TX_INPUT));
    txs[0].value += 1;
    // input transaction
    memcpy(&txs[1], &PETER_VECTOR.bundle[1], sizeof(TX_INPUT));
    // meta transaction
    memcpy(&txs[2], &PETER_VECTOR.bundle[2], sizeof(TX_INPUT));

    // create a valid bundle
    finalize_bundle(txs, 2);

    api_initialize();
    EXPECT_API_SET_SEED_OK(PETER_VECTOR.seed, security);
    { // output transaction
        TX_OUTPUT output = {0};
        output.finalized = false;

        EXPECT_API_DATA_OK(tx, 0, txs[0], output);
    }
    { // input transaction
        TX_OUTPUT output = {0};
        output.finalized = false;

        EXPECT_API_DATA_OK(tx, 0, txs[1], output);
    }
    { // meta transaction
        EXPECT_API_EXCEPTION(tx, 0, txs[2]);
    }
}

static void test_payment_lower_than_balance(void **state)
{
    UNUSED(state);
    static const int security = 2;

    TX_INPUT txs[8];
    // output transaction
    memcpy(&txs[0], &PETER_VECTOR.bundle[0], sizeof(TX_INPUT));
    // input transaction
    memcpy(&txs[1], &PETER_VECTOR.bundle[1], sizeof(TX_INPUT));
    txs[1].value += 1;
    // meta transaction
    memcpy(&txs[2], &PETER_VECTOR.bundle[2], sizeof(TX_INPUT));

    // create a valid bundle
    finalize_bundle(txs, 2);

    api_initialize();
    EXPECT_API_SET_SEED_OK(PETER_VECTOR.seed, security);
    { // output transaction
        TX_OUTPUT output = {0};
        output.finalized = false;

        EXPECT_API_DATA_OK(tx, 0, txs[0], output);
    }
    { // input transaction
        TX_OUTPUT output = {0};
        output.finalized = false;

        EXPECT_API_DATA_OK(tx, 0, txs[1], output);
    }
    { // meta transaction
        EXPECT_API_EXCEPTION(tx, 0, txs[2]);
    }
}

static void test_invalid_input_address_index(void **state)
{
    UNUSED(state);
    static const int security = 2;

    TX_INPUT txs[8];
    // output transaction
    memcpy(&txs[0], &PETER_VECTOR.bundle[0], sizeof(TX_INPUT));
    // input transaction
    memcpy(&txs[1], &PETER_VECTOR.bundle[1], sizeof(TX_INPUT));
    txs[1].address_idx += 1;
    // meta transaction
    memcpy(&txs[2], &PETER_VECTOR.bundle[2], sizeof(TX_INPUT));
    txs[2].address_idx += 1;

    // create a valid bundle
    finalize_bundle(txs, 2);

    api_initialize();
    EXPECT_API_SET_SEED_OK(PETER_VECTOR.seed, security);
    { // output transaction
        TX_OUTPUT output = {0};
        output.finalized = false;

        EXPECT_API_DATA_OK(tx, 0, txs[0], output);
    }
    { // input transaction
        TX_OUTPUT output = {0};
        output.finalized = false;

        EXPECT_API_DATA_OK(tx, 0, txs[1], output);
    }
    { // meta transaction
        EXPECT_API_EXCEPTION(tx, 0, txs[2]);
    }
}

static void test_invalid_tx_order(void **state)
{
    UNUSED(state);
    static const int security = 2;

    api_initialize();
    EXPECT_API_SET_SEED_OK(PETER_VECTOR.seed, security);
    { // input transaction as the first transaction
        TX_INPUT input;
        memcpy(&input, &PETER_VECTOR.bundle[1], sizeof(input));
        input.current_index = 0;

        EXPECT_API_EXCEPTION(tx, 0, input);
    }
}

static void test_tx_index_twice(void **state)
{
    UNUSED(state);
    static const int security = 2;

    api_initialize();
    EXPECT_API_SET_SEED_OK(PETER_VECTOR.seed, security);
    {
        TX_INPUT input;
        memcpy(&input, &PETER_VECTOR.bundle[0], sizeof(input));
        TX_OUTPUT output = {0};
        output.finalized = false;

        EXPECT_API_DATA_OK(tx, 0, input, output);
    }
    {
        TX_INPUT input;
        memcpy(&input, &PETER_VECTOR.bundle[1], sizeof(input));
        input.current_index = 0;

        EXPECT_API_EXCEPTION(tx, 0, input);
    }
}

static void test_missing_meta_tx(void **state)
{
    UNUSED(state);
    static const int security = 2;

    TX_INPUT txs[8];
    // output transaction
    memcpy(&txs[0], &PETER_VECTOR.bundle[0], sizeof(TX_INPUT));
    // input transaction
    memcpy(&txs[1], &PETER_VECTOR.bundle[1], sizeof(TX_INPUT));

    // create a valid bundle
    finalize_bundle(txs, 1);

    api_initialize();
    EXPECT_API_SET_SEED_OK(PETER_VECTOR.seed, security);
    { // output transaction
        TX_OUTPUT output = {0};
        output.finalized = false;

        EXPECT_API_DATA_OK(tx, 0, txs[0], output);
    }
    { // input transaction
        EXPECT_API_EXCEPTION(tx, 0, txs[1]);
    }
}

static void test_missing_meta_tx_with_change(void **state)
{
    UNUSED(state);
    static const int security = 2;

    TX_INPUT txs[8];
    // output transaction
    memcpy(&txs[0], &PETER_VECTOR.bundle[0], sizeof(TX_INPUT));
    // input transaction
    memcpy(&txs[1], &PETER_VECTOR.bundle[1], sizeof(TX_INPUT));
    // 0-value change transaction
    memcpy(&txs[2], &PETER_VECTOR.bundle[0], sizeof(TX_INPUT));
    txs[2].value = 0;

    // create a valid bundle
    finalize_bundle(txs, 2);

    api_initialize();
    EXPECT_API_SET_SEED_OK(PETER_VECTOR.seed, security);
    { // output transaction
        TX_OUTPUT output = {0};
        output.finalized = false;

        EXPECT_API_DATA_OK(tx, 0, txs[0], output);
    }
    { // input transaction
        TX_OUTPUT output = {0};
        output.finalized = false;

        EXPECT_API_DATA_OK(tx, 0, txs[1], output);
    }
    { // 0-value change transaction
        EXPECT_API_EXCEPTION(tx, 0, txs[2]);
    }
}

static void test_meta_tx_without_reference(void **state)
{
    UNUSED(state);
    static const int security = 2;

    api_initialize();
    EXPECT_API_SET_SEED_OK(PETER_VECTOR.seed, security);

    int tx_index = 0;
    const int last_index = 2;
    { // output transaction
        TX_INPUT input;
        memcpy(&input, &PETER_VECTOR.bundle[0], sizeof(input));
        input.current_index = tx_index++;
        input.last_index = last_index;

        TX_OUTPUT output = {0};
        output.finalized = false;

        EXPECT_API_DATA_OK(tx, 0, input, output);
    }
    { // meta transaction
        TX_INPUT input;
        memcpy(&input, &PETER_VECTOR.bundle[2], sizeof(input));
        input.current_index = tx_index++;
        input.last_index = last_index;

        EXPECT_API_EXCEPTION(tx, 0, input);
    }
}

static void test_invalid_change_index(void **state)
{
    UNUSED(state);
    static const int security = 2;

    TX_INPUT txs[8];
    // output transaction
    memcpy(&txs[0], &PETER_VECTOR.bundle[0], sizeof(TX_INPUT));
    txs[0].value -= 1;
    // input transaction
    memcpy(&txs[1], &PETER_VECTOR.bundle[1], sizeof(TX_INPUT));
    // meta transaction
    memcpy(&txs[2], &PETER_VECTOR.bundle[2], sizeof(TX_INPUT));
    // valued change transaction
    memcpy(&txs[3], &PETER_VECTOR.bundle[1], sizeof(TX_INPUT));
    memcpy(txs[3].address, PETER_VECTOR.addresses[security][3],
           NUM_HASH_TRYTES);
    txs[3].value = 1;
    txs[3].address_idx = 9999;

    // create a valid bundle
    finalize_bundle(txs, 3);

    api_initialize();
    EXPECT_API_SET_SEED_OK(PETER_VECTOR.seed, security);
    { // output transaction
        TX_OUTPUT output = {0};
        output.finalized = false;

        EXPECT_API_DATA_OK(tx, 0, txs[0], output);
    }
    { // input transaction
        TX_OUTPUT output = {0};
        output.finalized = false;

        EXPECT_API_DATA_OK(tx, 0, txs[1], output);
    }
    { // meta transaction
        TX_OUTPUT output = {0};
        output.finalized = false;

        EXPECT_API_DATA_OK(tx, 0, txs[2], output);
    }
    { // 0-value change transaction
        EXPECT_API_EXCEPTION(tx, 0, txs[3]);
    }
}

static void test_output_address_reuses_input(void **state)
{
    UNUSED(state);
    static const int security = 2;

    TX_INPUT txs[8];
    // output transaction with input address
    memcpy(&txs[0], &PETER_VECTOR.bundle[0], sizeof(TX_INPUT));
    memcpy(txs[0].address, PETER_VECTOR.bundle[1].address, NUM_HASH_TRYTES);
    // input transaction
    memcpy(&txs[1], &PETER_VECTOR.bundle[1], sizeof(TX_INPUT));
    // meta transaction
    memcpy(&txs[2], &PETER_VECTOR.bundle[2], sizeof(TX_INPUT));

    // create a valid bundle
    finalize_bundle(txs, 2);

    api_initialize();
    EXPECT_API_SET_SEED_OK(PETER_VECTOR.seed, security);
    { // output transaction
        TX_OUTPUT output = {0};
        output.finalized = false;

        EXPECT_API_DATA_OK(tx, 0, txs[0], output);
    }
    { // input transaction
        TX_OUTPUT output = {0};
        output.finalized = false;

        EXPECT_API_DATA_OK(tx, 0, txs[1], output);
    }
    { // meta transaction
        EXPECT_API_EXCEPTION(tx, 0, txs[2]);
    }
}

static void test_change_index_low(void **state)
{
    UNUSED(state);
    static const int security = 2;

    TX_INPUT txs[8];
    // output transaction
    memcpy(&txs[0], &PETER_VECTOR.bundle[0], sizeof(TX_INPUT));
    txs[0].value -= 1;
    // input transaction
    memcpy(&txs[1], &PETER_VECTOR.bundle[1], sizeof(TX_INPUT));
    // meta transaction
    memcpy(&txs[2], &PETER_VECTOR.bundle[2], sizeof(TX_INPUT));
    {
        // use output as boilerplate
        memcpy(&txs[3], &PETER_VECTOR.bundle[0], sizeof(TX_INPUT));
        // lower than the input
        txs[3].address_idx = txs[1].address_idx - 1;
        memcpy(txs[3].address,
               &PETER_VECTOR.addresses[security][txs[3].address_idx],
               NUM_HASH_TRYTES);
        txs[3].value = 1;
    }

    // create a valid bundle
    finalize_bundle(txs, 3);

    api_initialize();
    EXPECT_API_SET_SEED_OK(PETER_VECTOR.seed, security);
    { // output transaction
        TX_OUTPUT output = {0};
        output.finalized = false;

        EXPECT_API_DATA_OK(tx, 0, txs[0], output);
    }
    { // input transaction
        TX_OUTPUT output = {0};
        output.finalized = false;

        EXPECT_API_DATA_OK(tx, 0, txs[1], output);
    }
    { // meta transaction
        TX_OUTPUT output = {0};
        output.finalized = false;

        EXPECT_API_DATA_OK(tx, 0, txs[2], output);
    }
    { // invalid change transaction
        EXPECT_API_EXCEPTION(tx, 0, txs[3]);
    }
}

static void test_invalid_value_transaction(void **state)
{
    UNUSED(state);
    static const int security = 2;

    api_initialize();
    EXPECT_API_SET_SEED_OK(PETER_VECTOR.seed, security);
    { // output transaction
        TX_INPUT input;
        memcpy(&input, &PETER_VECTOR.bundle[0], sizeof(input));
        input.value = MAX_IOTA_VALUE + 1;

        EXPECT_API_EXCEPTION(tx, 0, input);
    }
}

static void test_seed_not_set(void **state)
{
    UNUSED(state);

    api_initialize();
    {
        TX_INPUT input;
        memcpy(&input, &PETER_VECTOR.bundle[0], sizeof(input));

        EXPECT_API_EXCEPTION(tx, 0, input);
    }
}

static void test_tx_after_bundle(void **state)
{
    UNUSED(state);

    api_initialize();

    EXPECT_API_SET_SEED_OK(PETER_VECTOR.seed, 2);
    EXPECT_API_SET_BUNDLE_OK(PETER_VECTOR.bundle, 2, PETER_VECTOR.bundle_hash);
    {
        TX_INPUT input;
        memcpy(&input, &PETER_VECTOR.bundle[0], sizeof(input));

        EXPECT_API_EXCEPTION(tx, 0, input);
    }
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_bundles_for_seed_from_file),
        cmocka_unit_test(test_refinalize_valid_bundle),
        cmocka_unit_test(test_invalid_input_address_index),
        cmocka_unit_test(test_invalid_tx_order),
        cmocka_unit_test(test_tx_index_twice),
        cmocka_unit_test(test_payment_higher_than_balance),
        cmocka_unit_test(test_payment_lower_than_balance),
        cmocka_unit_test(test_missing_meta_tx),
        cmocka_unit_test(test_missing_meta_tx_with_change),
        cmocka_unit_test(test_meta_tx_without_reference),
        cmocka_unit_test(test_invalid_change_index),
        cmocka_unit_test(test_output_address_reuses_input),
        cmocka_unit_test(test_change_index_low),
        cmocka_unit_test(test_invalid_value_transaction),
        cmocka_unit_test(test_seed_not_set),
        cmocka_unit_test(test_tx_after_bundle)};

    return cmocka_run_group_tests(tests, NULL, NULL);
}
