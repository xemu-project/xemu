/*
 * Validate -readconfig
 *
 * Copyright (c) 2022 Red Hat, Inc.
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 */

#include "qemu/osdep.h"
#include "libqtest.h"
#include "qapi/error.h"
#include "qapi/qapi-visit-machine.h"
#include "qapi/qapi-visit-qom.h"
#include "qapi/qapi-visit-ui.h"
#include "qapi/qmp/qdict.h"
#include "qapi/qmp/qlist.h"
#include "qapi/qobject-input-visitor.h"
#include "qapi/qmp/qstring.h"
#include "qemu/units.h"

static QTestState *qtest_init_with_config(const char *cfgdata)
{
    GError *error = NULL;
    g_autofree char *args = NULL;
    int cfgfd = -1;
    g_autofree char *cfgpath = NULL;
    QTestState *qts;
    ssize_t ret;

    cfgfd = g_file_open_tmp("readconfig-test-XXXXXX", &cfgpath, &error);
    g_assert_no_error(error);
    g_assert_cmpint(cfgfd, >=, 0);

    ret = qemu_write_full(cfgfd, cfgdata, strlen(cfgdata));
    close(cfgfd);
    if (ret < 0) {
        unlink(cfgpath);
    }
    g_assert_cmpint(ret, ==, strlen(cfgdata));

    args = g_strdup_printf("-nodefaults -machine none -readconfig %s", cfgpath);

    qts = qtest_init(args);

    unlink(cfgpath);

    return qts;
}

static void test_x86_memdev_resp(QObject *res)
{
    Visitor *v;
    g_autoptr(MemdevList) memdevs = NULL;
    Memdev *memdev;

    g_assert(res);
    v = qobject_input_visitor_new(res);
    visit_type_MemdevList(v, NULL, &memdevs, &error_abort);

    g_assert(memdevs);
    g_assert(memdevs->value);
    g_assert(!memdevs->next);

    memdev = memdevs->value;
    g_assert_cmpstr(memdev->id, ==, "ram");
    g_assert_cmpint(memdev->size, ==, 200 * MiB);

    visit_free(v);
}

static void test_x86_memdev(void)
{
    QDict *resp;
    QTestState *qts;
    const char *cfgdata =
        "[memory]\n"
        "size = \"200\"";

    qts = qtest_init_with_config(cfgdata);
    /* Test valid command */
    resp = qtest_qmp(qts, "{ 'execute': 'query-memdev' }");
    test_x86_memdev_resp(qdict_get(resp, "return"));
    qobject_unref(resp);

    qtest_quit(qts);
}


#ifdef CONFIG_SPICE
static void test_spice_resp(QObject *res)
{
    Visitor *v;
    g_autoptr(SpiceInfo) spice = NULL;

    g_assert(res);
    v = qobject_input_visitor_new(res);
    visit_type_SpiceInfo(v, "spice", &spice, &error_abort);

    g_assert(spice);
    g_assert(spice->enabled);

    visit_free(v);
}

static void test_spice(void)
{
    QDict *resp;
    QTestState *qts;
    const char *cfgdata =
        "[spice]\n"
        "disable-ticketing = \"on\"\n"
        "unix = \"on\"\n";

    qts = qtest_init_with_config(cfgdata);
    /* Test valid command */
    resp = qtest_qmp(qts, "{ 'execute': 'query-spice' }");
    test_spice_resp(qdict_get(resp, "return"));
    qobject_unref(resp);

    qtest_quit(qts);
}
#endif

static void test_object_rng_resp(QObject *res)
{
    Visitor *v;
    g_autoptr(ObjectPropertyInfoList) objs = NULL;
    ObjectPropertyInfoList *tmp;
    ObjectPropertyInfo *obj;
    bool seen_rng = false;

    g_assert(res);
    v = qobject_input_visitor_new(res);
    visit_type_ObjectPropertyInfoList(v, NULL, &objs, &error_abort);

    g_assert(objs);
    tmp = objs;
    while (tmp) {
        g_assert(tmp->value);

        obj = tmp->value;
        if (g_str_equal(obj->name, "rng0") &&
            g_str_equal(obj->type, "child<rng-builtin>")) {
            seen_rng = true;
            break;
        }

        tmp = tmp->next;
    }

    g_assert(seen_rng);

    visit_free(v);
}

static void test_object_rng(void)
{
    QDict *resp;
    QTestState *qts;
    const char *cfgdata =
        "[object]\n"
        "qom-type = \"rng-builtin\"\n"
        "id = \"rng0\"\n";

    qts = qtest_init_with_config(cfgdata);
    /* Test valid command */
    resp = qtest_qmp(qts,
                     "{ 'execute': 'qom-list',"
                     "  'arguments': {'path': '/objects' }}");
    test_object_rng_resp(qdict_get(resp, "return"));
    qobject_unref(resp);

    qtest_quit(qts);
}

int main(int argc, char *argv[])
{
    const char *arch;
    g_test_init(&argc, &argv, NULL);

    arch = qtest_get_arch();

    if (g_str_equal(arch, "i386") ||
        g_str_equal(arch, "x86_64")) {
        qtest_add_func("readconfig/x86/memdev", test_x86_memdev);
    }
#ifdef CONFIG_SPICE
    qtest_add_func("readconfig/spice", test_spice);
#endif

    qtest_add_func("readconfig/object-rng", test_object_rng);

    return g_test_run();
}
