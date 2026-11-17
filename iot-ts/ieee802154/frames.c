/** @file
 * @brief IEEE 802.15.4 Group
 * Build an 802.15.4 data frame with AES-CCM* security, parse it back and
 * assert the MIC, the security fields and the payload all round-trip.
 * Copyright (C) 2026 Interpretica Unipessoal Lda
 */

/*
 * Pure engine-side codec: build a frame, protect its payload with
 * AES-CCM*, lay it out on the wire, parse the wire back, and verify what
 * came out against what went in. Nothing here needs a radio, so it runs
 * on a plain localhost. The payload-size sweep at the end emits an MI
 * line-graph of the on-wire frame length against the payload length.
 */

#define TE_TEST_NAME    "ieee802154/frames"

#include "te_config.h"

#include <string.h>

#include "tapi_test.h"
#include "te_string.h"
#include "te_mi_log.h"

#include "tapi_802154.h"
#include "tapi_802154_sec.h"

/** The sender's extended address: it is baked into the CCM* nonce. */
#define FRAMES_SRC_ADDR 0x0011223344556677ULL
/** A fixed frame counter; the sweep round-trips are independent. */
#define FRAMES_COUNTER  0x0000002AU
/** Which key the frame names. */
#define FRAMES_KEY_INDEX 1

/** The 16-byte key every frame in this test is protected with. */
static const char *const frames_key_hex =
    "000102030405060708090A0B0C0D0E0F";

/** Payload sizes swept for the line-graph, in bytes. */
static const size_t frames_sizes[] = { 4, 8, 16, 24, 32, 48, 64 };

/**
 * The security levels that carry an integrity code, which is every
 * level worth round-tripping: a bare-encrypt level has nothing to check.
 */
static const tapi_802154_sec_level frames_levels[] = {
    TAPI_802154_SEC_MIC32,
    TAPI_802154_SEC_MIC64,
    TAPI_802154_SEC_MIC128,
    TAPI_802154_SEC_ENC_MIC32,
    TAPI_802154_SEC_ENC_MIC64,
    TAPI_802154_SEC_ENC_MIC128,
};

/** A template for a secured data frame from #FRAMES_SRC_ADDR. */
static tapi_802154_frame
frames_template(tapi_802154_sec_level level)
{
    tapi_802154_frame f = tapi_802154_default_frame;

    f.type = TAPI_802154_DATA;
    f.seq = 0x42;
    f.dest_mode = TAPI_802154_ADDR_SHORT;
    f.dest_panid = 0x1234;
    f.dest_addr = 0xABCD;
    f.src_mode = TAPI_802154_ADDR_EXTENDED;
    f.panid_compression = true;
    f.src_addr = FRAMES_SRC_ADDR;
    f.secured = true;
    f.sec_level = level;
    f.frame_counter = FRAMES_COUNTER;
    f.key_index = FRAMES_KEY_INDEX;

    return f;
}

/**
 * The MAC header plus the auxiliary security header, as bytes. CCM*
 * signs those (they are what a router reads to route the frame), so the
 * same bytes have to be handed to both halves of the security. They are
 * obtained by building the frame with no payload and dropping the two
 * checksum bytes, which are not part of what is authenticated.
 */
static void
frames_header(const tapi_802154_frame *tmpl, uint8_t *hdr, size_t *hdr_len)
{
    tapi_802154_frame f = *tmpl;
    te_string wire = TE_STRING_INIT;
    uint8_t bytes[TAPI_802154_MAX_FRAME];
    size_t n = 0;

    CHECK_RC(tapi_802154_build(&f, NULL, 0, &wire));
    CHECK_RC(tapi_802154_unhex(wire.ptr, bytes, sizeof(bytes), &n));
    te_string_free(&wire);

    *hdr_len = n - TAPI_802154_FCS_LEN;
    memcpy(hdr, bytes, *hdr_len);
}

/**
 * Protect @p plain, build a frame around it, parse the frame back and
 * recover the plaintext. Asserts the whole way through, and returns the
 * on-wire frame length in bytes.
 */
static size_t
frames_roundtrip(const uint8_t *key, tapi_802154_sec_level level,
                 const uint8_t *plain, size_t plain_len)
{
    tapi_802154_frame tmpl = frames_template(level);
    tapi_802154_frame got;
    uint8_t nonce[TAPI_802154_NONCE_LEN];
    uint8_t hdr[TAPI_802154_MAX_FRAME];
    size_t hdr_len = 0;
    uint8_t buf[TAPI_802154_MAX_FRAME];
    size_t buf_len = plain_len;
    uint8_t raw[TAPI_802154_MAX_FRAME];
    size_t raw_len = 0;
    uint8_t dec[TAPI_802154_MAX_FRAME];
    size_t dec_len;
    te_string wire = TE_STRING_INIT;
    size_t mic = tapi_802154_mic_len(level);

    frames_header(&tmpl, hdr, &hdr_len);

    /* Protect the payload: encrypt (when the level encrypts) and append
     * the integrity code. */
    tapi_802154_nonce(FRAMES_SRC_ADDR, FRAMES_COUNTER, level, nonce);
    memcpy(buf, plain, plain_len);
    CHECK_RC(tapi_802154_secure(key, nonce, level, hdr, hdr_len, buf,
                                &buf_len, sizeof(buf)));
    if (buf_len != plain_len + mic)
        TEST_VERDICT("secure() made the payload %zu bytes, expected %zu",
                     buf_len, plain_len + mic);
    if (tapi_802154_encrypts(level) && plain_len != 0 &&
        memcmp(buf, plain, plain_len) == 0)
        TEST_VERDICT("the payload was not encrypted at level %u", level);

    /* Lay the protected payload out on the wire. */
    CHECK_RC(tapi_802154_build(&tmpl, buf, buf_len, &wire));
    CHECK_RC(tapi_802154_unhex(wire.ptr, raw, sizeof(raw), &raw_len));

    /* Parse it straight back. */
    CHECK_RC(tapi_802154_parse(raw, raw_len, &got));
    if (!got.fcs_valid)
        TEST_VERDICT("the parsed frame failed its checksum");
    if (got.sec_level != level)
        TEST_VERDICT("security level came back %u, expected %u",
                     got.sec_level, level);
    if (got.frame_counter != FRAMES_COUNTER)
        TEST_VERDICT("frame counter came back %u", got.frame_counter);
    if (got.key_index != FRAMES_KEY_INDEX)
        TEST_VERDICT("key index came back %u", got.key_index);
    if (got.src_addr != FRAMES_SRC_ADDR)
        TEST_VERDICT("source address did not round-trip");
    if (got.payload_len != buf_len ||
        memcmp(got.payload, buf, buf_len) != 0)
        TEST_VERDICT("the protected payload did not round-trip");

    /* Recover the plaintext: this is where the MIC is checked. */
    memcpy(dec, got.payload, got.payload_len);
    dec_len = got.payload_len;
    CHECK_RC(tapi_802154_unsecure(key, nonce, level, hdr, hdr_len, dec,
                                  &dec_len));
    if (dec_len != plain_len || memcmp(dec, plain, plain_len) != 0)
        TEST_VERDICT("the recovered plaintext does not match the original");

    te_string_free(&wire);
    return raw_len;
}

int
main(int argc, char **argv)
{
    te_mi_logger *logger = NULL;
    uint8_t key[TAPI_802154_KEY_LEN];
    size_t key_len = 0;
    uint8_t plain[TAPI_802154_MAX_FRAME];
    size_t i;
    tapi_802154_sec_level level;

    TEST_START;

    TEST_STEP("Prepare the network key and a payload pattern");
    CHECK_RC(tapi_802154_unhex(frames_key_hex, key, sizeof(key), &key_len));
    if (key_len != TAPI_802154_KEY_LEN)
        TEST_VERDICT("the key is %zu bytes, expected %d", key_len,
                     TAPI_802154_KEY_LEN);
    for (i = 0; i < sizeof(plain); i++)
        plain[i] = (uint8_t)(0xA0 + (i & 0x0f));

    TEST_STEP("An unsecured data frame round-trips through the codec");
    {
        tapi_802154_frame f = tapi_802154_default_frame;
        tapi_802154_frame got;
        uint8_t raw[TAPI_802154_MAX_FRAME];
        size_t raw_len = 0;
        te_string wire = TE_STRING_INIT;
        static const uint8_t body[] = "payload";

        f.dest_panid = 0x1234;
        f.dest_addr = 0xABCD;
        f.src_addr = 0x1111;
        CHECK_RC(tapi_802154_build(&f, body, sizeof(body) - 1, &wire));
        CHECK_RC(tapi_802154_unhex(wire.ptr, raw, sizeof(raw), &raw_len));
        CHECK_RC(tapi_802154_parse(raw, raw_len, &got));
        tapi_802154_frame_log("unsecured", &got);
        if (!got.fcs_valid)
            TEST_VERDICT("the unsecured frame failed its checksum");
        if (got.secured)
            TEST_VERDICT("an unsecured frame parsed as secured");
        if (got.dest_addr != 0xABCD || got.src_addr != 0x1111)
            TEST_VERDICT("the addresses did not round-trip");
        if (got.payload_len != sizeof(body) - 1 ||
            memcmp(got.payload, body, sizeof(body) - 1) != 0)
            TEST_VERDICT("the payload did not round-trip");
        te_string_free(&wire);
    }

    TEST_STEP("Every AES-CCM* security level round-trips");
    for (i = 0; i < TE_ARRAY_LEN(frames_levels); i++)
    {
        size_t len;

        level = frames_levels[i];
        len = frames_roundtrip(key, level, plain, 24);
        RING("level %u (%s, %zu-byte MIC): frame is %zu bytes on the wire",
             level, tapi_802154_encrypts(level) ? "encrypted" : "signed",
             tapi_802154_mic_len(level), len);
    }

    TEST_STEP("A tampered frame is rejected by its integrity code");
    {
        tapi_802154_sec_level lvl = TAPI_802154_SEC_ENC_MIC128;
        tapi_802154_frame tmpl = frames_template(lvl);
        uint8_t nonce[TAPI_802154_NONCE_LEN];
        uint8_t hdr[TAPI_802154_MAX_FRAME];
        size_t hdr_len = 0;
        uint8_t buf[TAPI_802154_MAX_FRAME];
        size_t buf_len = 16;
        size_t dec_len;
        te_errno rc;

        frames_header(&tmpl, hdr, &hdr_len);
        tapi_802154_nonce(FRAMES_SRC_ADDR, FRAMES_COUNTER, lvl, nonce);
        memcpy(buf, plain, buf_len);
        CHECK_RC(tapi_802154_secure(key, nonce, lvl, hdr, hdr_len, buf,
                                    &buf_len, sizeof(buf)));
        buf[0] ^= 0xff; /* flip a ciphertext byte */
        dec_len = buf_len;
        rc = tapi_802154_unsecure(key, nonce, lvl, hdr, hdr_len, buf,
                                  &dec_len);
        if (TE_RC_GET_ERROR(rc) != TE_EBADMSG)
            TEST_VERDICT("a tampered frame was accepted (rc=%r)", rc);
        RING("tampered frame rejected with TE_EBADMSG, as it should be");
    }

    TEST_STEP("Sweep payload sizes and graph the on-wire frame length");
    CHECK_RC(te_mi_logger_meas_create("tapi_802154", &logger));
    te_mi_logger_add_meas_key(logger, NULL, "sec_level", "%u",
                              TAPI_802154_SEC_ENC_MIC128);
    for (i = 0; i < TE_ARRAY_LEN(frames_sizes); i++)
    {
        size_t sz = frames_sizes[i];
        size_t wire_len = frames_roundtrip(key, TAPI_802154_SEC_ENC_MIC128,
                                           plain, sz);

        te_mi_logger_add_meas(logger, NULL, TE_MI_MEAS_UNITLESS_VALUE,
                              "payload", TE_MI_MEAS_AGGR_SINGLE,
                              (double)sz, TE_MI_MEAS_MULTIPLIER_PLAIN);
        te_mi_logger_add_meas(logger, NULL, TE_MI_MEAS_MEMORY,
                              "frame", TE_MI_MEAS_AGGR_SINGLE,
                              (double)wire_len, TE_MI_MEAS_MULTIPLIER_PLAIN);
    }
    te_mi_logger_add_meas_view(logger, NULL, TE_MI_MEAS_VIEW_LINE_GRAPH,
                               "frame_size",
                               "On-wire frame length vs. payload length");
    te_mi_logger_meas_graph_axis_add_name(logger, NULL,
                               TE_MI_MEAS_VIEW_LINE_GRAPH, "frame_size",
                               TE_MI_GRAPH_AXIS_X, "payload");
    te_mi_logger_meas_graph_axis_add(logger, NULL,
                               TE_MI_MEAS_VIEW_LINE_GRAPH, "frame_size",
                               TE_MI_GRAPH_AXIS_Y, TE_MI_MEAS_MEMORY,
                               "frame");
    te_mi_logger_destroy(logger);
    logger = NULL;

    TEST_SUCCESS;

cleanup:
    if (logger != NULL)
        te_mi_logger_destroy(logger);
    TEST_END;
}
