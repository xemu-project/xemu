#include "qemu/osdep.h"
#include "qapi/error.h"
#include "block/nbd.h"
#include "io/channel-socket.h"
#include "qemu/bswap.h"

/*
 * Minimal NBD new-style handshake: INIT_MAGIC (8) + OPTS_MAGIC (8) + flags (2)
 * The client reads these 18 bytes before doing anything with info->name, so
 * the stub must write them before the parent calls nbd_receive_negotiate().
 */
static void write_nbd_handshake(int fd)
{
    uint64_t init_magic = cpu_to_be64(NBD_INIT_MAGIC);
    uint64_t opts_magic = cpu_to_be64(NBD_OPTS_MAGIC);
    uint16_t flags = cpu_to_be16(NBD_FLAG_FIXED_NEWSTYLE | NBD_FLAG_NO_ZEROES);
    uint32_t client_flags;

    signal(SIGPIPE, SIG_IGN);
    write(fd, &init_magic, sizeof(init_magic));
    write(fd, &opts_magic, sizeof(opts_magic));
    write(fd, &flags, sizeof(flags));
    /* drain client flags; ignore errors — client may close early */
    read(fd, &client_flags, sizeof(client_flags));
}

/*
 * nbd_receive_negotiate() must reject info->name longer than NBD_MAX_STRING_SIZE
 * before sending any option data (the check fires in nbd_opt_info_or_go before
 * any bytes are written to the server).
 */
static void test_export_name_too_long(void)
{
    int sv[2];
    pid_t pid;
    int wstatus;

    if (socketpair(PF_UNIX, SOCK_STREAM, 0, sv) < 0) {
        g_test_skip("socketpair unavailable");
        return;
    }

    pid = fork();
    if (pid == 0) {
        close(sv[0]);
        write_nbd_handshake(sv[1]);
        close(sv[1]);
        _exit(0);
    }
    close(sv[1]);

    QIOChannelSocket *sioc = qio_channel_socket_new_fd(sv[0], &error_abort);
    char *long_name = g_strnfill(NBD_MAX_STRING_SIZE + 1, 'x');
    NBDExportInfo info = {
        .name = long_name,
        .mode = NBD_MODE_SIMPLE,
        .request_sizes = false,
        .base_allocation = false,
    };
    Error *err = NULL;

    int ret = nbd_receive_negotiate(QIO_CHANNEL(sioc), NULL, NULL, NULL,
                                    &info, &err);

    g_assert_cmpint(ret, <, 0);
    g_assert_nonnull(err);
    g_assert_nonnull(strstr(error_get_pretty(err), "too long"));

    error_free(err);
    g_free(long_name);
    object_unref(OBJECT(sioc));
    waitpid(pid, &wstatus, 0);
}

/*
 * A name exactly at the limit must not be rejected by the length guard.
 * The negotiation will fail for other reasons (server stub closes after
 * handshake), but the error must not be "export name too long".
 */
static void test_export_name_at_limit(void)
{
    int sv[2];
    pid_t pid;
    int wstatus;

    if (socketpair(PF_UNIX, SOCK_STREAM, 0, sv) < 0) {
        g_test_skip("socketpair unavailable");
        return;
    }

    pid = fork();
    if (pid == 0) {
        close(sv[0]);
        write_nbd_handshake(sv[1]);
        close(sv[1]);
        _exit(0);
    }
    close(sv[1]);

    QIOChannelSocket *sioc = qio_channel_socket_new_fd(sv[0], &error_abort);
    char *max_name = g_strnfill(NBD_MAX_STRING_SIZE, 'x');
    NBDExportInfo info = {
        .name = max_name,
        .mode = NBD_MODE_SIMPLE,
        .request_sizes = false,
        .base_allocation = false,
    };
    Error *err = NULL;

    int ret = nbd_receive_negotiate(QIO_CHANNEL(sioc), NULL, NULL, NULL,
                                    &info, &err);

    /* Must fail (server stub closed), but NOT because name was too long */
    g_assert_cmpint(ret, <, 0);
    g_assert_nonnull(err);
    g_assert_null(strstr(error_get_pretty(err), "too long"));

    error_free(err);
    g_free(max_name);
    object_unref(OBJECT(sioc));
    waitpid(pid, &wstatus, 0);
}

int main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);

    g_test_add_func("/nbd/client/export-name-too-long", test_export_name_too_long);
    g_test_add_func("/nbd/client/export-name-at-limit", test_export_name_at_limit);

    return g_test_run();
}
