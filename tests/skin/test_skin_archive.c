#include "skin_preview.h"

#include <glib.h>
#include <glib/gstdio.h>

#include "skin_archive.h"

static gchar *skin_test_fixture_path(const gchar *name)
{
    return g_build_filename(SKIN_TEST_FIXTURE_DIR, name, NULL);
}

static gchar *skin_test_golden_path(const gchar *name)
{
    return g_build_filename(SKIN_TEST_GOLDEN_DIR, name, NULL);
}

static guint64 skin_test_hash_fixture(const gchar *fixture_name, GError **error)
{
    gchar *fixture = skin_test_fixture_path(fixture_name);
    gchar *temp_png = g_build_filename(g_get_tmp_dir(), "xmms-skin-preview-test.png", NULL);
    guint64 hash = 0;

    if (skin_preview_write_png(fixture, temp_png, error))
        hash = skin_preview_png_ahash(temp_png, error);

    g_remove(temp_png);
    g_free(temp_png);
    g_free(fixture);
    return hash;
}

static guint64 skin_test_hash_golden(const gchar *golden_name, GError **error)
{
    gchar *golden = skin_test_golden_path(golden_name);
    guint64 hash = skin_preview_png_ahash(golden, error);

    g_free(golden);
    return hash;
}

static void skin_test_cleanup_dir(const gchar *path)
{
    gchar *argv[] = {(gchar *)"rm", (gchar *)"-rf", (gchar *)path, NULL};

    g_spawn_sync(NULL, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, NULL, NULL, NULL);
}

static void test_skin_archive_path_guard(void)
{
    g_assert_true(skin_archive_path_is_safe("MAIN.BMP"));
    g_assert_true(skin_archive_path_is_safe("Skins/Classic/eqmain.bmp"));
    g_assert_false(skin_archive_path_is_safe("../../.bashrc"));
    g_assert_false(skin_archive_path_is_safe("/tmp/main.bmp"));
    g_assert_false(skin_archive_path_is_safe("C:\\Windows\\main.bmp"));
    g_assert_false(skin_archive_path_is_safe("skins\\..\\escape.bmp"));
}

static void test_skin_archive_extract_classic(void)
{
    GError *error = NULL;
    gchar *fixture = skin_test_fixture_path("classic.wsz");
    gchar *tmpdir = skin_archive_extract_to_tempdir(fixture, &error);
    gchar *main_bmp;

    g_assert_no_error(error);
    g_assert_nonnull(tmpdir);

    main_bmp = g_build_filename(tmpdir, "main.bmp", NULL);
    g_assert_true(g_file_test(main_bmp, G_FILE_TEST_EXISTS));

    g_free(main_bmp);
    skin_test_cleanup_dir(tmpdir);
    g_free(tmpdir);
    g_free(fixture);
}

static void test_skin_archive_rejects_traversal(void)
{
    GError *error = NULL;
    gchar *fixture = skin_test_fixture_path("malicious-traversal.wsz");
    gchar *tmpdir = skin_archive_extract_to_tempdir(fixture, &error);

    g_assert_null(tmpdir);
    g_assert_error(error, G_FILE_ERROR, G_FILE_ERROR_INVAL);

    g_clear_error(&error);
    g_free(fixture);
}

static void test_skin_archive_rejects_corrupt_archive(void)
{
    GError *error = NULL;
    gchar *fixture = skin_test_fixture_path("corrupt.wsz");
    gchar *tmpdir = skin_archive_extract_to_tempdir(fixture, &error);

    g_assert_null(tmpdir);
    g_assert_nonnull(error);

    g_clear_error(&error);
    g_free(fixture);
}

static void test_skin_preview_matches_classic_golden(void)
{
    GError *error = NULL;
    guint64 fixture_hash = skin_test_hash_fixture("classic.wsz", &error);
    guint64 golden_hash = skin_test_hash_golden("classic.png", &error);

    g_assert_no_error(error);
    g_assert_cmpuint(skin_preview_hamming_distance(fixture_hash, golden_hash), <=, 0);
}

static void test_skin_preview_matches_missing_eqmain_golden(void)
{
    GError *error = NULL;
    guint64 fixture_hash = skin_test_hash_fixture("missing-eqmain.wsz", &error);
    guint64 golden_hash = skin_test_hash_golden("missing-eqmain.png", &error);

    g_assert_no_error(error);
    g_assert_cmpuint(skin_preview_hamming_distance(fixture_hash, golden_hash), <=, 0);
}

static void test_skin_preview_matches_custom_overlay_golden(void)
{
    GError *error = NULL;
    guint64 fixture_hash = skin_test_hash_fixture("custom-overlay.wsz", &error);
    guint64 golden_hash = skin_test_hash_golden("custom-overlay.png", &error);

    g_assert_no_error(error);
    g_assert_cmpuint(skin_preview_hamming_distance(fixture_hash, golden_hash), <=, 0);
}

int main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);

    g_test_add_func("/skin/archive/path-guard", test_skin_archive_path_guard);
    g_test_add_func("/skin/archive/extract-classic", test_skin_archive_extract_classic);
    g_test_add_func("/skin/archive/reject-traversal", test_skin_archive_rejects_traversal);
    g_test_add_func("/skin/archive/reject-corrupt", test_skin_archive_rejects_corrupt_archive);
    g_test_add_func("/skin/preview/classic", test_skin_preview_matches_classic_golden);
    g_test_add_func("/skin/preview/missing-eqmain", test_skin_preview_matches_missing_eqmain_golden);
    g_test_add_func("/skin/preview/custom-overlay", test_skin_preview_matches_custom_overlay_golden);

    return g_test_run();
}
