#include "prots.h"

#ifdef HAVE_SSL

static SSL_CTX *ssl_ctx = NULL;

int init_ssl(void)
{
    if (ssl_ctx)
        return 1;

    SSL_library_init();
    SSL_load_error_strings();
    OpenSSL_add_all_algorithms();

    ssl_ctx = SSL_CTX_new(SSLv23_client_method());
    if (!ssl_ctx) {
        fprintf(stderr, "> SSL_CTX_new failed\n");
        return 0;
    }

    /* Disable ancient protocols */
    SSL_CTX_set_options(ssl_ctx, SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3 | SSL_OP_NO_COMPRESSION);
    /* For cloner use-case we currently skip cert verification; can be tightened later */
    SSL_CTX_set_verify(ssl_ctx, SSL_VERIFY_NONE, NULL);

    return 1;
}

void cleanup_ssl(void)
{
    if (ssl_ctx) {
        SSL_CTX_free(ssl_ctx);
        ssl_ctx = NULL;
    }
}

int ssl_connect_clone(struct clone_connection *conn)
{
    SSL *ssl;

    if (!ssl_ctx || !conn || conn->sock <= 0)
        return -1;

    ssl = SSL_new(ssl_ctx);
    if (!ssl)
        return -1;

    SSL_set_fd(ssl, conn->sock);
    SSL_set_connect_state(ssl);
    conn->ssl = ssl;

    /* Non-blocking: SSL_connect will be driven from the event loop */
    return 0;
}

int ssl_read(void *ssl, char *buf, int len)
{
    int ret;

    if (!ssl)
        return -1;

    ret = SSL_read((SSL *)ssl, buf, len);
    if (ret <= 0) {
        int err = SSL_get_error((SSL *)ssl, ret);
        if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE)
            return 0; /* try again later */
        return -1;
    }
    return ret;
}

int ssl_write(void *ssl, const char *buf, int len)
{
    int ret;

    if (!ssl)
        return -1;

    ret = SSL_write((SSL *)ssl, buf, len);
    if (ret <= 0) {
        int err = SSL_get_error((SSL *)ssl, ret);
        if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE)
            return 0;
        return -1;
    }
    return ret;
}

#endif /* HAVE_SSL */
