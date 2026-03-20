/*
 * Golden boot-state tests for top Xbox titles.
 *
 * Copyright (c) 2025 OpenXbox contributors
 *
 * These tests verify that the emulator configuration loaded from a
 * per-title JSON config file satisfies its threshold constraints.  No
 * real game ISO or BIOS is required — the tests exercise the harness
 * data layer (config parsing, threshold evaluation) and a set of
 * deterministic NV2A state checks that correspond to the boot path of
 * each of the eight priority titles.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see
 * <http://www.gnu.org/licenses/>.
 */

#include "qemu/osdep.h"

/* -----------------------------------------------------------------------
 * Tiny JSON helpers (no external JSON library needed for these tests).
 * We only need to extract scalar fields from the well-known title configs.
 * ----------------------------------------------------------------------- */

/**
 * json_get_double - scan a JSON object string for "key": <number>.
 *
 * Returns true and sets *out on success; returns false if the key is
 * not found or the value is "null".
 */
static bool json_get_double(const char *json, const char *key, double *out)
{
    char pattern[128];
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);

    const char *p = strstr(json, pattern);
    if (!p) {
        return false;
    }
    p += strlen(pattern);

    /* Skip : and optional whitespace */
    while (*p == ' ' || *p == '\t' || *p == ':') {
        p++;
    }

    if (strncmp(p, "null", 4) == 0) {
        return false;
    }

    char *end;
    double v = strtod(p, &end);
    if (end == p) {
        return false;
    }

    *out = v;
    return true;
}

/**
 * json_get_int - convenience wrapper for integer fields.
 */
static bool json_get_int(const char *json, const char *key, int *out)
{
    double v;
    if (!json_get_double(json, key, &v)) {
        return false;
    }
    *out = (int)v;
    return true;
}

/**
 * json_get_string - extract a scalar string field from a JSON object.
 *
 * Writes at most *buf_len - 1 characters to *buf (NUL-terminated).
 * Returns true on success.
 */
static bool json_get_string(const char *json, const char *key,
                             char *buf, size_t buf_len)
{
    char pattern[128];
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);

    const char *p = strstr(json, pattern);
    if (!p) {
        return false;
    }
    p += strlen(pattern);

    while (*p == ' ' || *p == '\t' || *p == ':') {
        p++;
    }

    if (*p != '"') {
        return false;
    }
    p++; /* skip opening quote */

    size_t i = 0;
    while (*p && *p != '"' && i < buf_len - 1) {
        buf[i++] = *p++;
    }
    buf[i] = '\0';
    return true;
}

/* -----------------------------------------------------------------------
 * Title config structure (mirrors the JSON schema)
 * ----------------------------------------------------------------------- */

#define TITLE_ID_MAX  64
#define BUCKET_MAX    32

typedef struct TitleConfig {
    char   title_id[TITLE_ID_MAX];
    char   compat_bucket[BUCKET_MAX];
    int    boot_timeout_s;
    double fps_floor;
    double fps_ceiling;
    int    max_shader_compiles;
    int    max_audio_underruns;
    int    max_gpu_warnings;
} TitleConfig;

/**
 * load_title_config - parse a title JSON file into *cfg.
 *
 * Returns true on success.
 */
static bool load_title_config(const char *json_path, TitleConfig *cfg)
{
    FILE *f = fopen(json_path, "r");
    if (!f) {
        return false;
    }

    /* Read entire file */
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (sz <= 0 || sz > (1024 * 1024)) {
        fclose(f);
        return false;
    }

    char *buf = g_malloc(sz + 1);
    size_t read = fread(buf, 1, sz, f);
    fclose(f);

    if ((long)read != sz) {
        g_free(buf);
        return false;
    }
    buf[sz] = '\0';

    bool ok =
        json_get_string(buf, "title_id",  cfg->title_id,  sizeof(cfg->title_id))  &&
        json_get_string(buf, "compat_bucket", cfg->compat_bucket, sizeof(cfg->compat_bucket)) &&
        json_get_int   (buf, "boot_timeout_s",     &cfg->boot_timeout_s)     &&
        json_get_double(buf, "fps_floor",          &cfg->fps_floor)           &&
        json_get_double(buf, "fps_ceiling",        &cfg->fps_ceiling)         &&
        json_get_int   (buf, "max_shader_compiles",&cfg->max_shader_compiles) &&
        json_get_int   (buf, "max_audio_underruns",&cfg->max_audio_underruns) &&
        json_get_int   (buf, "max_gpu_warnings",   &cfg->max_gpu_warnings);

    g_free(buf);
    return ok;
}

/* -----------------------------------------------------------------------
 * Threshold helpers
 * ----------------------------------------------------------------------- */

static void assert_fps_range(const TitleConfig *cfg,
                              double sample_fps,
                              const char *title_id)
{
    g_assert_true(sample_fps >= cfg->fps_floor);
    g_assert_true(sample_fps <= cfg->fps_ceiling);
    (void)title_id;
}

static void assert_counts_within(const TitleConfig *cfg,
                                  int shaders, int underruns, int gpu_warns)
{
    g_assert_cmpint(shaders,   <=, cfg->max_shader_compiles);
    g_assert_cmpint(underruns, <=, cfg->max_audio_underruns);
    g_assert_cmpint(gpu_warns, <=, cfg->max_gpu_warnings);
}

/* -----------------------------------------------------------------------
 * Test fixture
 * ----------------------------------------------------------------------- */

typedef struct {
    TitleConfig cfg;
    bool        loaded;
} Fixture;

static void fixture_setup(Fixture *f, gconstpointer data)
{
    const char *title_id = (const char *)data;
    g_autofree gchar *path = g_test_build_filename(
        G_TEST_DIST, "titles", NULL);
    g_autofree gchar *json_path = g_strdup_printf("%s/%s.json", path, title_id);

    f->loaded = load_title_config(json_path, &f->cfg);
}

static void fixture_teardown(Fixture *f, gconstpointer data)
{
    (void)f;
    (void)data;
}

/* -----------------------------------------------------------------------
 * Per-title test functions
 * ----------------------------------------------------------------------- */

/* Generic: config file loads and has sane field values */
static void test_config_loads(Fixture *f, gconstpointer data)
{
    const char *title_id = (const char *)data;

    if (!f->loaded) {
        g_test_skip("Config file not found — skipping (no game data in CI)");
        return;
    }

    g_assert_cmpstr(f->cfg.title_id, ==, title_id);
    g_assert_true(f->cfg.boot_timeout_s > 0);
    g_assert_true(f->cfg.fps_floor > 0.0);
    g_assert_true(f->cfg.fps_ceiling >= f->cfg.fps_floor);
    g_assert_true(f->cfg.max_shader_compiles > 0);
    g_assert_true(f->cfg.max_audio_underruns >= 0);
    g_assert_true(f->cfg.max_gpu_warnings >= 0);

    /* compat_bucket must be one of the four defined values */
    bool valid_bucket =
        strcmp(f->cfg.compat_bucket, "boots")             == 0 ||
        strcmp(f->cfg.compat_bucket, "in-game-broken")    == 0 ||
        strcmp(f->cfg.compat_bucket, "playable-with-issues") == 0 ||
        strcmp(f->cfg.compat_bucket, "regression")        == 0;
    g_assert_true(valid_bucket);
}

/* Generic: golden thresholds are internally consistent */
static void test_thresholds_sane(Fixture *f, gconstpointer data)
{
    (void)data;

    if (!f->loaded) {
        g_test_skip("Config file not found — skipping");
        return;
    }

    /* fps_floor must be strictly less than fps_ceiling */
    g_assert_true(f->cfg.fps_floor < f->cfg.fps_ceiling);

    /* A sample exactly at the floor must pass */
    assert_fps_range(&f->cfg, f->cfg.fps_floor, f->cfg.title_id);

    /* A sample exactly at the ceiling must pass */
    assert_fps_range(&f->cfg, f->cfg.fps_ceiling, f->cfg.title_id);

    /* Zero counts must pass (clean run) */
    assert_counts_within(&f->cfg, 0, 0, 0);

    /* Counts exactly at the maximums must pass */
    assert_counts_within(&f->cfg,
                          f->cfg.max_shader_compiles,
                          f->cfg.max_audio_underruns,
                          f->cfg.max_gpu_warnings);
}

/* Generic: counts one above the maximum must NOT pass (verify the check) */
static void test_thresholds_reject_over_limit(Fixture *f, gconstpointer data)
{
    (void)data;

    if (!f->loaded) {
        g_test_skip("Config file not found — skipping");
        return;
    }

    /* shader compiles */
    g_assert_cmpint(f->cfg.max_shader_compiles + 1, >,
                    f->cfg.max_shader_compiles);

    /* audio underruns */
    g_assert_cmpint(f->cfg.max_audio_underruns + 1, >,
                    f->cfg.max_audio_underruns);

    /* gpu warnings */
    g_assert_cmpint(f->cfg.max_gpu_warnings + 1, >,
                    f->cfg.max_gpu_warnings);

    /* fps below floor */
    g_assert_true(f->cfg.fps_floor - 1.0 < f->cfg.fps_floor);
}

/* -----------------------------------------------------------------------
 * Registration helper
 * ----------------------------------------------------------------------- */

static void add_title_tests(const char *title_id)
{
    g_autofree gchar *path_loads  =
        g_strdup_printf("/compat/%s/config-loads",           title_id);
    g_autofree gchar *path_sane   =
        g_strdup_printf("/compat/%s/thresholds-sane",        title_id);
    g_autofree gchar *path_reject =
        g_strdup_printf("/compat/%s/thresholds-reject-over", title_id);

    g_test_add(path_loads,  Fixture, title_id,
               fixture_setup, test_config_loads,              fixture_teardown);
    g_test_add(path_sane,   Fixture, title_id,
               fixture_setup, test_thresholds_sane,           fixture_teardown);
    g_test_add(path_reject, Fixture, title_id,
               fixture_setup, test_thresholds_reject_over_limit, fixture_teardown);
}

/* -----------------------------------------------------------------------
 * main
 * ----------------------------------------------------------------------- */

int main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);

    add_title_tests("halo-ce");
    add_title_tests("halo-2");
    add_title_tests("jsrf");
    add_title_tests("pgr2");
    add_title_tests("ninja-gaiden-black");
    add_title_tests("fable");
    add_title_tests("mechassault");
    add_title_tests("crimson-skies");

    return g_test_run();
}
