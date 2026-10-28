/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Host tests of the device-independent part: run by tools/check.sh. */
#include <stdio.h>
#include <string.h>

#include "../src/mm.h"

static int failures, checks;
#define CHECK(c)                                                         \
    do {                                                                 \
        checks++;                                                        \
        if (!(c)) {                                                      \
            failures++;                                                  \
            printf("%s:%d: %s: CHECK(%s)\n", __FILE__, __LINE__, __func__, #c); \
        }                                                                \
    } while (0)

static const mm_dest table[] = {
    {25, MM_G_SRC, 0x400, 0x7c00, "TUNE"}, {26, MM_G_NONE, 0, 0, 0}, {30, MM_G_FLTR, 0, 0, "FREQ"},
    {55, MM_G_FX, 0, 0, "DEL"},            {64, MM_G_FX, 0, 0x6400, 0},
};
static const mm_dests dests = {table, 5, 70};

static void fresh(mm_t *s)
{
    memset(s, 0, sizeof *s);
    s->dests = &dests;
}

static int press(mm_t *s, int code) { return mm_key(s, code, MM_F_PRESS); }
static int release(mm_t *s, int code) { return mm_key(s, code, MM_F_RELEASE); }
static int repeat(mm_t *s, int code) { return mm_key(s, code, MM_F_PRESS | MM_F_REPEAT); }

static void enter(mm_t *s)
{
    press(s, MM_K_PAGE);
    press(s, MM_K_MOD);
    release(s, MM_K_MOD);
    release(s, MM_K_PAGE);
}

static void test_entry_page_then_mod(void)
{
    mm_t s;
    fresh(&s);
    CHECK(press(&s, MM_K_PAGE) == 0);   /* the firmware sees PAGE: it cannot know MOD follows */
    CHECK(s.state == MM_OFF);
    CHECK(press(&s, MM_K_MOD) == 1);    /* taken: the LFO page does not open */
    CHECK(s.state == MM_MACRO);
    CHECK(release(&s, MM_K_MOD) == 1);  /* a release follows its press */
    CHECK(release(&s, MM_K_PAGE) == 0);
    CHECK(s.state == MM_MACRO);
}

static void test_entry_mod_then_page(void)
{
    mm_t s;
    fresh(&s);
    CHECK(press(&s, MM_K_MOD) == 0);    /* the firmware opens the LFO page */
    CHECK(press(&s, MM_K_PAGE) == 1);
    CHECK(s.state == MM_MACRO);
    CHECK(release(&s, MM_K_PAGE) == 1);
    CHECK(release(&s, MM_K_MOD) == 0);
}

static void test_no_entry_without_chord(void)
{
    mm_t s;
    fresh(&s);
    CHECK(press(&s, MM_K_PAGE) == 0);
    CHECK(release(&s, MM_K_PAGE) == 0);
    CHECK(press(&s, MM_K_MOD) == 0);    /* PAGE was let go: no chord */
    CHECK(s.state == MM_OFF);
    CHECK(release(&s, MM_K_MOD) == 0);
    CHECK(press(&s, MM_K_TRIG1) == 0);
    CHECK(mm_enc(&s, 1, 3) == 0);       /* off: every encoder is the firmware's */
}

static int refuse(void) { return 0; }

static void test_entry_refused_by_view(void)
{
    mm_t s;
    fresh(&s);
    s.can_enter = refuse;               /* D9: a menu or dialog is open */
    press(&s, MM_K_PAGE);
    CHECK(press(&s, MM_K_MOD) == 0);
    CHECK(s.state == MM_OFF);
}

static void test_exit_on_mod_press(void)
{
    mm_t s;
    fresh(&s);
    enter(&s);
    CHECK(press(&s, MM_K_MOD) == 1);
    CHECK(s.state == MM_OFF);           /* on the press, not the release (D15) */
    CHECK(release(&s, MM_K_MOD) == 1);
}

static void test_exit_mod_then_page_does_not_reenter(void)
{
    mm_t s;
    fresh(&s);
    enter(&s);
    CHECK(press(&s, MM_K_MOD) == 1);    /* exits; this MOD press never starts a chord */
    CHECK(press(&s, MM_K_PAGE) == 0);   /* the firmware's PAGE, as usual */
    CHECK(s.state == MM_OFF);
    CHECK(release(&s, MM_K_PAGE) == 0);
    CHECK(release(&s, MM_K_MOD) == 1);
}

static void test_exit_page_then_mod(void)
{
    mm_t s;
    fresh(&s);
    enter(&s);
    CHECK(press(&s, MM_K_PAGE) == 1);   /* taken in macro mode */
    CHECK(press(&s, MM_K_MOD) == 1);
    CHECK(s.state == MM_OFF);
    CHECK(release(&s, MM_K_PAGE) == 1);
    CHECK(release(&s, MM_K_MOD) == 1);
    CHECK(press(&s, MM_K_PAGE) == 0);   /* off again: PAGE is the firmware's */
}

static void test_pass_through(void)
{
    mm_t s;
    fresh(&s);
    enter(&s);
    int pass[] = {MM_K_PLAY, MM_K_STOP, MM_K_RECORD, MM_K_PTN, MM_K_SONG, MM_K_TRK, MM_K_FUNC, MM_K_TRIG1,
                  MM_K_TRIG1 + 15, MM_K_LEFT, MM_K_RIGHT, MM_K_NO};
    for (unsigned i = 0; i < sizeof pass / sizeof pass[0]; i++) {
        CHECK(press(&s, pass[i]) == 0);
        CHECK(repeat(&s, pass[i]) == 0);
        CHECK(release(&s, pass[i]) == 0);
    }
    int taken[] = {MM_K_YES, MM_K_UP, MM_K_DOWN, 1, 2, 3, 4, 5, 7, 8, 9, 18, 49, 50, 52};
    for (unsigned i = 0; i < sizeof taken / sizeof taken[0]; i++) {
        CHECK(press(&s, taken[i]) == 1);
        CHECK(repeat(&s, taken[i]) == 1);
        CHECK(release(&s, taken[i]) == 1);
    }
    CHECK(mm_enc(&s, 12, 2) == 0);            /* an encoder the mode does not know stays the firmware's */
    CHECK(s.state == MM_MACRO);
}

static void test_release_of_key_held_before_entry(void)
{
    mm_t s;
    fresh(&s);
    CHECK(press(&s, MM_K_PUSH_A + 2) == 0);   /* held before the mode starts */
    enter(&s);
    CHECK(release(&s, MM_K_PUSH_A + 2) == 0); /* its press went to the firmware, so does its release */
    CHECK(s.pushes == 0);
}

static void test_turns_and_coarse(void)
{
    mm_t s;
    fresh(&s);
    enter(&s);
    CHECK(mm_enc(&s, 3, 5) == 1);
    CHECK(s.m[2].pos == 5);
    CHECK(s.popup == 3);
    CHECK(press(&s, MM_K_PUSH_A + 2) == 1);   /* push + turn: bigger steps (D3) */
    mm_enc(&s, 3, 2);
    CHECK(s.m[2].pos == 5 + 2 * MM_COARSE);
    release(&s, MM_K_PUSH_A + 2);
    mm_enc(&s, 3, 1000);
    CHECK(s.m[2].pos == MM_POS_MAX);
    mm_enc(&s, 3, -1000);
    CHECK(s.m[2].pos == MM_POS_MIN);
    CHECK(mm_enc(&s, 1, 0) == 0);
}

static void test_push_no_resets(void)
{
    mm_t s;
    fresh(&s);
    enter(&s);
    mm_enc(&s, 1, 10);
    mm_enc(&s, 2, -10);
    press(&s, MM_K_PUSH_A);
    CHECK(press(&s, MM_K_NO) == 1);
    CHECK(release(&s, MM_K_NO) == 1);
    CHECK(s.m[0].pos == 0);             /* back to the centre */
    CHECK(s.m[1].pos == -10);           /* B was not pushed */
    release(&s, MM_K_PUSH_A);
    CHECK(press(&s, MM_K_NO) == 0);     /* NO alone is the firmware's on the macro screen */
}

static void test_state_kept_after_exit(void)
{
    mm_t s;
    fresh(&s);
    enter(&s);
    mm_enc(&s, 4, 7);
    press(&s, MM_K_MOD);
    release(&s, MM_K_MOD);
    enter(&s);
    CHECK(s.m[3].pos == 7);             /* D2 */
}

static void test_setup_open_edit_and_back(void)
{
    mm_t s;
    fresh(&s);
    enter(&s);
    press(&s, MM_K_FUNC);
    CHECK(press(&s, MM_K_PUSH_A + 2) == 1);   /* FUNC + push C */
    CHECK(s.state == MM_SETUP && s.edit == 2);
    release(&s, MM_K_PUSH_A + 2);
    CHECK(release(&s, MM_K_FUNC) == 0);
    CHECK(mm_enc(&s, 2, 3) == 1);       /* B: slot 2 destination, three steps */
    CHECK(s.m[2].slot[1].dest == 30);
    CHECK(s.sel == 1);
    mm_enc(&s, 6, -12);                 /* F: slot 2 depth */
    CHECK(s.m[2].slot[1].depth == -12);
    CHECK(press(&s, MM_K_TRIG1 + 2) == 1);
    CHECK(press(&s, MM_K_TRIG1 + 4) == 1);
    CHECK(release(&s, MM_K_TRIG1 + 4) == 1);
    CHECK(s.m[2].slot[1].tracks == ((1u << 2) | (1u << 4)));
    CHECK(press(&s, MM_K_TRIG1 + 2) == 1);    /* toggles off */
    CHECK(s.m[2].slot[1].tracks == (1u << 4));
    CHECK(press(&s, MM_K_PLAY) == 0);
    s.dirty_config = 0;
    CHECK(press(&s, MM_K_NO) == 1);
    CHECK(s.state == MM_MACRO && s.dirty_config);
}

static void test_setup_dest_bounds(void)
{
    mm_t s;
    fresh(&s);
    enter(&s);
    press(&s, MM_K_FUNC);
    press(&s, MM_K_PUSH_A);
    release(&s, MM_K_PUSH_A);
    release(&s, MM_K_FUNC);
    mm_enc(&s, 1, 100);
    CHECK(s.m[0].slot[0].dest == 64);   /* the last destination */
    mm_enc(&s, 1, -100);
    CHECK(s.m[0].slot[0].dest == 0);    /* none */
    mm_enc(&s, 5, 1000);
    CHECK(s.m[0].slot[0].depth == MM_POS_MAX);
}

static void test_setup_mod_goes_back_twice_to_leave(void)
{
    mm_t s;
    fresh(&s);
    enter(&s);
    press(&s, MM_K_FUNC);
    press(&s, MM_K_PUSH_A + 7);
    release(&s, MM_K_PUSH_A + 7);
    release(&s, MM_K_FUNC);
    CHECK(s.state == MM_SETUP);
    press(&s, MM_K_PAGE);               /* PAGE held + MOD on the setup screen: back, not out (D8) */
    CHECK(press(&s, MM_K_MOD) == 1);
    CHECK(s.state == MM_MACRO);
    release(&s, MM_K_MOD);
    release(&s, MM_K_PAGE);
    CHECK(press(&s, MM_K_MOD) == 1);    /* the second MOD press leaves */
    CHECK(s.state == MM_OFF);
}

static void test_setup_push_no_left_right_func_switch(void)
{
    mm_t s;
    fresh(&s);
    enter(&s);
    press(&s, MM_K_FUNC);
    press(&s, MM_K_PUSH_A);
    release(&s, MM_K_PUSH_A);
    release(&s, MM_K_FUNC);
    mm_enc(&s, 1, 2);
    mm_enc(&s, 5, 9);
    press(&s, MM_K_PUSH_A + 4);         /* push E + NO: slot 1 depth to 0, its destination stays */
    press(&s, MM_K_NO);
    release(&s, MM_K_NO);
    release(&s, MM_K_PUSH_A + 4);
    CHECK(s.m[0].slot[0].depth == 0 && s.m[0].slot[0].dest == 26);
    CHECK(s.state == MM_SETUP);
    press(&s, MM_K_PUSH_A);             /* push A + NO: destination to none */
    press(&s, MM_K_NO);
    release(&s, MM_K_PUSH_A);
    CHECK(s.m[0].slot[0].dest == 0);
    press(&s, MM_K_RIGHT);
    press(&s, MM_K_RIGHT);
    CHECK(s.sel == 2);
    for (int i = 0; i < 5; i++)
        press(&s, MM_K_RIGHT);
    CHECK(s.sel == 3);
    press(&s, MM_K_FUNC);
    press(&s, MM_K_PUSH_A + 5);         /* FUNC + push F: the setup of macro F */
    CHECK(s.state == MM_SETUP && s.edit == 5);
}

static void test_slot_delta(void)
{
    CHECK(mm_slot_delta(0, 63) == 0);
    CHECK(mm_slot_delta(63, 0) == 0);
    CHECK(mm_slot_delta(-64, -64) == MM_RAW_MAX);
    CHECK(mm_slot_delta(63, 63) == (63 * 63 * MM_RAW_MAX) >> 12);
    CHECK(mm_slot_delta(-63, 63) == -mm_slot_delta(63, 63));
    CHECK(mm_slot_delta(1, 1) == MM_RAW_MAX >> 12);
}

static void test_build_and_apply(void)
{
    mm_t s;
    static mm_mods mods;
    fresh(&s);
    s.m[0].pos = 16;
    s.m[0].slot[0] = (mm_slot){30, 63, 0x0005};  /* tracks 1 and 3 */
    s.m[0].slot[1] = (mm_slot){30, 63, 0x0001};  /* the same destination on track 1: merged */
    s.m[1].pos = 0;                              /* at the centre: nothing */
    s.m[1].slot[0] = (mm_slot){55, 63, 0xffff};
    s.m[2].pos = -64;
    s.m[2].slot[0] = (mm_slot){64, 32, 0x0004};  /* track 3 */
    s.m[3].pos = 10;
    s.m[3].slot[0] = (mm_slot){90, 63, 0xffff};  /* at or above the bound: never applied */
    mm_build(&s, &mods);
    CHECK(mods.active == 0x0005);
    CHECK(mods.t[0].n == 1 && mods.t[0].dest[0] == 30 && mods.t[0].delta[0] == 2 * mm_slot_delta(16, 63));
    CHECK(mods.t[2].n == 2);
    CHECK(mods.t[1].n == 0);

    int16_t values[MM_TRACKS * 71];
    for (int i = 0; i < MM_TRACKS * 71; i++)
        values[i] = 0x4000;
    values[2 * 71 + 64] = 0x0100;
    mm_apply(&mods, values, 71, 70);
    CHECK(values[30] == 0x4000 + 2 * mm_slot_delta(16, 63));
    CHECK(values[2 * 71 + 30] == 0x4000 + mm_slot_delta(16, 63));
    CHECK(values[2 * 71 + 64] == 0);            /* saturated at 0 */
    CHECK(values[1 * 71 + 30] == 0x4000);       /* track 2 is in no mask */
    CHECK(values[3 * 71 + 90 % 71] == 0x4000);

    for (int i = 0; i < MM_MACROS; i++)
        s.m[i].pos = 0;                         /* every macro at the centre: nothing to do */
    mm_build(&s, &mods);
    CHECK(mods.active == 0);
}

static void test_build_saturates_and_cancels(void)
{
    mm_t s;
    static mm_mods mods;
    fresh(&s);
    for (int k = 0; k < 4; k++) {
        s.m[k].pos = 63;
        s.m[k].slot[0] = (mm_slot){25, 63, 1};
    }
    s.m[4].pos = 20;
    s.m[4].slot[0] = (mm_slot){26, 30, 1};
    s.m[5].pos = -20;
    s.m[5].slot[0] = (mm_slot){26, 30, 1};
    mm_build(&s, &mods);
    CHECK(mods.t[0].n == 1);                    /* 26 cancels out */
    CHECK(mods.t[0].delta[0] == MM_RAW_MAX);
    int16_t v[71] = {0};
    v[25] = 0x7000;
    mm_apply(&mods, v, 71, 70);
    CHECK(v[25] == 0x7c00);                     /* held in the destination's range */
    for (int k = 0; k < 4; k++)
        s.m[k].pos = -64;
    mm_build(&s, &mods);
    v[25] = 0x7000;
    mm_apply(&mods, v, 71, 70);
    CHECK(v[25] == 0x400);
}

static void test_level_moves_all(void)
{
    mm_t s;
    fresh(&s);
    CHECK(mm_enc(&s, MM_ENC_LEVEL, 3) == 0);    /* off: LEVEL is the firmware's */
    enter(&s);
    s.m[1].pos = 62;
    s.m[2].pos = -10;
    s.dirty_mods = 0;
    CHECK(mm_enc(&s, MM_ENC_LEVEL, 3) == 1);
    CHECK(s.m[0].pos == 3 && s.m[1].pos == MM_POS_MAX && s.m[2].pos == -7 && s.m[7].pos == 3);
    CHECK(s.dirty_mods && s.dirty_screen);
    mm_enc(&s, MM_ENC_LEVEL, -200);
    for (int k = 0; k < MM_MACROS; k++)
        CHECK(s.m[k].pos == MM_POS_MIN);
}

static void test_func_no_centres_all(void)
{
    mm_t s;
    fresh(&s);
    enter(&s);
    for (int k = 0; k < MM_MACROS; k++)
        s.m[k].pos = (int8_t)(k * 5 - 20);
    s.m[2].slot[0] = (mm_slot){30, 10, 1};
    press(&s, MM_K_FUNC);
    CHECK(press(&s, MM_K_NO) == 1);             /* FUNC + NO: taken, so the pattern is not reloaded */
    CHECK(release(&s, MM_K_NO) == 1);
    release(&s, MM_K_FUNC);
    for (int k = 0; k < MM_MACROS; k++)
        CHECK(s.m[k].pos == 0);
    CHECK(s.state == MM_MACRO && s.m[2].slot[0].dest == 30);

    s.m[5].pos = 9;                             /* on the setup screen too, without leaving it */
    press(&s, MM_K_FUNC);
    press(&s, MM_K_PUSH_A + 1);
    release(&s, MM_K_PUSH_A + 1);
    CHECK(s.state == MM_SETUP);
    CHECK(press(&s, MM_K_NO) == 1);
    release(&s, MM_K_NO);
    release(&s, MM_K_FUNC);
    CHECK(s.m[5].pos == 0 && s.state == MM_SETUP);
    CHECK(mm_key(&s, MM_K_NO, MM_F_PRESS | MM_F_FUNC) == 1); /* the firmware's FUNC bit alone counts too */
}

static void test_leds_dirty(void)
{
    mm_t s;
    fresh(&s);
    enter(&s);
    CHECK(s.dirty_leds);                /* entering lights MOD */
    s.dirty_leds = 0;
    mm_enc(&s, 1, 3);
    CHECK(!s.dirty_leds);               /* a macro turn changes no LED */
    press(&s, MM_K_FUNC);
    press(&s, MM_K_PUSH_A);
    release(&s, MM_K_FUNC);
    CHECK(s.dirty_leds);                /* the setup screen shows the tracks */
    s.dirty_leds = 0;
    press(&s, MM_K_TRIG1);
    CHECK(s.dirty_leds);
    s.dirty_leds = 0;
    press(&s, MM_K_RIGHT);
    CHECK(s.dirty_leds);                /* another slot, other tracks */
    s.dirty_leds = 0;
    press(&s, MM_K_MOD);
    CHECK(s.dirty_leds);
    s.dirty_leds = 0;
    press(&s, MM_K_MOD);
    CHECK(s.dirty_leds && s.state == MM_OFF); /* leaving gives the LEDs back */
}

static void test_leds(void)
{
    mm_t s;
    uint8_t code[20], val[20];
    fresh(&s);
    CHECK(mm_leds(&s, code, val, 20) == 0);
    enter(&s);
    CHECK(mm_leds(&s, code, val, 20) == 1 && code[0] == MM_K_MOD && val[0] == MM_LED_BLUE);
    press(&s, MM_K_FUNC);
    press(&s, MM_K_PUSH_A);
    release(&s, MM_K_FUNC);
    s.m[0].slot[0].tracks = 0x8001;
    CHECK(mm_leds(&s, code, val, 20) == 17);
    CHECK(code[1] == MM_K_TRIG1 && val[1] == MM_LED_RED);
    CHECK(val[2] == MM_LED_OFF);
    CHECK(code[16] == MM_K_TRIG1 + 15 && val[16] == MM_LED_RED);
}

static void test_blob_round_trip(void)
{
    mm_t a, b;
    uint8_t blob[MM_BLOB_SIZE];
    fresh(&a);
    fresh(&b);
    a.m[3].slot[2] = (mm_slot){55, -40, 0xa5a5};
    a.m[7].slot[3] = (mm_slot){99, 5, 1};       /* not in this list: decodes to none */
    a.m[7].pos = 12;                            /* positions are not stored (D14) */
    CHECK(mm_encode(&a, blob) == MM_BLOB_SIZE);
    CHECK(mm_decode(&b, blob, MM_BLOB_SIZE) == 0);
    CHECK(b.m[3].slot[2].dest == 55 && b.m[3].slot[2].depth == -40 && b.m[3].slot[2].tracks == 0xa5a5);
    CHECK(b.m[7].slot[3].dest == 0 && b.m[7].pos == 0);
    blob[10] ^= 1;
    CHECK(mm_decode(&b, blob, MM_BLOB_SIZE) == -1);
    blob[10] ^= 1;
    blob[2] = MM_BLOB_VERSION + 1;              /* a future version is refused */
    CHECK(mm_decode(&b, blob, MM_BLOB_SIZE) == -1);
    CHECK(mm_decode(&b, blob, MM_BLOB_SIZE - 1) == -1);
}

static void test_record(void)
{
    mm_t a, b;
    static uint8_t r0[MM_RECORD_SIZE], r1[MM_RECORD_SIZE];
    uint32_t seq = 0;
    fresh(&a);
    fresh(&b);
    CHECK(mm_record_pick(&b, r0, r1, &seq) == -1);      /* a blank card: nothing to load */
    a.m[1].slot[0] = (mm_slot){30, 7, 0x0101};
    mm_record_encode(&a, 5, r0);
    CHECK(mm_record_pick(&b, r0, r1, &seq) == 0 && seq == 5);
    a.m[1].slot[0].depth = 9;
    mm_record_encode(&a, 6, r1);
    CHECK(mm_record_pick(&b, r0, r1, &seq) == 1 && seq == 6); /* the newer copy */
    r1[20] ^= 0x40;                                     /* a torn write: the older copy */
    CHECK(mm_record_pick(&b, r0, r1, &seq) == 0 && seq == 5);
    CHECK(mm_decode(&b, r0 + 8, MM_BLOB_SIZE) == 0 && b.m[1].slot[0].depth == 7);
    r0[0] = 'X';
    CHECK(mm_record_pick(&b, r0, r1, &seq) == -1);
}

static void test_labels(void)
{
    char buf[8];
    mm_dest_label(&dests, 30, buf);
    CHECK(!strcmp(buf, "FREQ"));
    mm_dest_label(&dests, 26, buf);
    CHECK(!strcmp(buf, "P26"));
    CHECK(!strcmp(mm_dest_group(&dests, 30), "FLTR"));
    CHECK(!strcmp(mm_dest_group(&dests, 0), "META"));
    CHECK(!strcmp(mm_dest_group(&dests, 26), "META"));
    mm_dest_label(&dests, 0, buf);
    CHECK(!strcmp(buf, "---"));
    mm_dest_label(0, 99, buf);
    CHECK(!strcmp(buf, "P99"));
}

void test_draw(int *checks_out, int *failures_out);

int main(void)
{
    test_entry_page_then_mod();
    test_entry_mod_then_page();
    test_no_entry_without_chord();
    test_entry_refused_by_view();
    test_exit_on_mod_press();
    test_exit_mod_then_page_does_not_reenter();
    test_exit_page_then_mod();
    test_pass_through();
    test_release_of_key_held_before_entry();
    test_turns_and_coarse();
    test_push_no_resets();
    test_state_kept_after_exit();
    test_setup_open_edit_and_back();
    test_setup_dest_bounds();
    test_setup_mod_goes_back_twice_to_leave();
    test_setup_push_no_left_right_func_switch();
    test_slot_delta();
    test_build_and_apply();
    test_build_saturates_and_cancels();
    test_level_moves_all();
    test_func_no_centres_all();
    test_leds();
    test_leds_dirty();
    test_blob_round_trip();
    test_labels();
    test_record();
    test_draw(&checks, &failures);
    printf("%d checks, %d failed\n", checks, failures);
    return failures != 0;
}
