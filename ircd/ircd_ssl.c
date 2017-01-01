/*
 * IRC-Dev IRCD - An advanced and innovative IRC Daemon, ircd/ircd_ssl.c
 *
 * Copyright (C) 2002-2017 IRC-Dev Development Team <devel@irc-dev.net>
 * Copyright (C) 2007 Toni Garcia (zoltan) <zoltan@irc-dev.net>
 * Copyright (C) 2002 Alex Badea <vampire@go.ro>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * $Id: ssl.c,v 1.1 2007-11-11 21:53:08 zolty Exp $
 *
 */
/** @file
 * @brief Implementation of SSL.
 * @version $Id: ssl.c,v 1.1 2007-11-11 21:53:08 zolty Exp $
 */
#include "config.h"

#if defined(USE_SSL)
#include "ircd_ssl.h"
#include "client.h"
#include "ircd.h"
#include "ircd_alloc.h"
#include "ircd_defs.h"
#include "ircd_events.h"
#include "ircd_log.h"
#include "ircd_snprintf.h"
#include "listener.h"
#include "s_bsd.h"
#include "s_debug.h"
#include "send.h"

#define _XOPEN_SOURCE
#include <fcntl.h>
#include <limits.h>
#include <sys/uio.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

RSA *server_rsa_private_key;
SSL_CTX *ctx;
static int ssl_inuse = 0;
int bio_spare_fd = -1;

struct ssl_data {
  struct Socket socket;
  struct Listener *listener;
  int fd;
};

int
save_spare_fd(const char *spare_purpose)
{
  int spare_fd = open("/dev/null", O_RDONLY, 0);

  if (spare_fd < 0)
  {
    log_write(LS_SYSTEM, L_INFO, 0, "Failed to reserve low fd for %s - open failed", spare_purpose);
    return -1;
  }
  else if (spare_fd > 255)
  {
    log_write(LS_SYSTEM, L_INFO, 0, "Failed to reserve low fd for %s - too high", spare_purpose);
    close(spare_fd);
    return -1;
  }

  return spare_fd;
}

static void abort_ssl(struct ssl_data *data)
{
  Debug((DEBUG_DEBUG, "SSL: aborted"));
  SSL_free(data->socket.s_ssl);
  --ssl_inuse;
  close(data->fd);
  socket_del(&data->socket);
}

static void accept_ssl(struct ssl_data *data)
{
  const char* const error_ssl = "ERROR :SSL connection error\r\n";

  if (SSL_accept(data->socket.s_ssl) <= 0) {
    unsigned long err = ERR_get_error();
    char string[120];

    if (err) {
      ERR_error_string(err, string);
      Debug((DEBUG_ERROR, "SSL_accept: %s", string));

      write(data->fd, error_ssl, strlen(error_ssl));

      abort_ssl(data);
    }
    return;
  }
  if (SSL_is_init_finished(data->socket.s_ssl)) {
    add_connection(data->listener, data->fd, data->socket.s_ssl);
    socket_del(&data->socket);
  }
}

static void ssl_sock_callback(struct Event* ev)
{
  struct ssl_data *data;

  assert(0 != ev_socket(ev));
  assert(0 != s_data(ev_socket(ev)));

  data = s_data(ev_socket(ev));
  assert(0 != data);

  switch (ev_type(ev)) {
  case ET_DESTROY:
    --data->listener->ref_count;
    MyFree(data);
    return;
  case ET_ERROR:
  case ET_EOF:
    abort_ssl(data);
    break;
  case ET_READ:
  case ET_WRITE:
    accept_ssl(data);
    break;
  default:
    break;
  }
}

void ssl_add_connection(struct Listener *listener, int fd)
{
  struct ssl_data *data;

  assert(0 != listener);

  if (!os_set_nonblocking(fd)) {
    close(fd);
    return;
  }
  os_disable_options(fd);

  data = (struct ssl_data *) MyMalloc(sizeof(struct ssl_data));
  data->listener = listener;
  data->fd = fd;
  if (!socket_add(&data->socket, ssl_sock_callback, (void *) data, SS_CONNECTED, SOCK_EVENT_READABLE, fd)) {
    close(fd);
    return;
  }
  if (!(data->socket.s_ssl = SSL_new(ctx))) {
    Debug((DEBUG_DEBUG, "SSL_new failed"));
    close(fd);
    return;
  }
  SSL_set_fd(data->socket.s_ssl, fd);
  ++ssl_inuse;
  ++listener->ref_count;
}

/*
 * ssl_recv - non blocking read of a connection
 * returns:
 *  1  if data was read or socket is blocked (recoverable error)
 *    count_out > 0 if data was read
 *
 *  0  if socket closed from other end
 *  -1 if an unrecoverable error occurred
 */
IOResult ssl_recv(struct Socket *socket, char* buf,
                 unsigned int length, unsigned int* count_out)
{
  int res;

  assert(0 != socket);
  assert(0 != buf);
  assert(0 != count_out);

  *count_out = 0;
  errno = 0;

  res = SSL_read(socket->s_ssl, buf, length);
  switch (SSL_get_error(socket->s_ssl, res)) {
  case SSL_ERROR_NONE:
    *count_out = (unsigned) res;
    return IO_SUCCESS;
  case SSL_ERROR_WANT_WRITE:
  case SSL_ERROR_WANT_READ:
  case SSL_ERROR_WANT_X509_LOOKUP:
    Debug((DEBUG_DEBUG, "SSL_read returned WANT_ - retrying"));
    return IO_BLOCKED;
  case SSL_ERROR_SYSCALL:
    if (res < 0 && errno == EINTR)
      return IO_BLOCKED; /* ??? */
    break;
  case SSL_ERROR_ZERO_RETURN: /* close_notify received */
    SSL_shutdown(socket->s_ssl); /* Send close_notify back */
    break;
  }
  return IO_FAILURE;
}

/*
 * ssl_sendv - non blocking writev to a connection
 * returns:
 *  1  if data was written
 *    count_out contains amount written
 *
 *  0  if write call blocked, recoverable error
 *  -1 if an unrecoverable error occurred
 */
IOResult ssl_sendv(struct Socket *socket, struct MsgQ* buf,
                  unsigned int* count_in, unsigned int* count_out)
{
  int res;
  int count;
  int k;
  struct iovec iov[IOV_MAX];
  IOResult retval = IO_BLOCKED;
  int ssl_err = 0;

  assert(0 != socket);
  assert(0 != buf);
  assert(0 != count_in);
  assert(0 != count_out);

  *count_in = 0;
  *count_out = 0;
  errno = 0;

  count = msgq_mapiov(buf, iov, IOV_MAX, count_in);
  for (k = 0; k < count; k++) {
    res = SSL_write(socket->s_ssl, iov[k].iov_base, iov[k].iov_len);
    ssl_err = SSL_get_error(socket->s_ssl, res);
    Debug((DEBUG_DEBUG, "SSL_write returned %d, error code %d.", res, ssl_err));
    switch (ssl_err) {
    case SSL_ERROR_NONE:
      *count_out += (unsigned) res;
      retval = IO_SUCCESS;
      break;
    case SSL_ERROR_WANT_WRITE:
    case SSL_ERROR_WANT_READ:
    case SSL_ERROR_WANT_X509_LOOKUP:
      Debug((DEBUG_DEBUG, "SSL_write returned want WRITE, READ, or X509; returning retval %d", retval));
      return retval;
    case SSL_ERROR_SSL:
      {
          int errorValue;
          Debug((DEBUG_ERROR, "SSL_write returned SSL_ERROR_SSL, errno %d, retval %d, res %d, ssl error code %d", errno, retval, res, ssl_err));
          ERR_load_crypto_strings();
          while((errorValue = ERR_get_error())) {
            Debug((DEBUG_ERROR, "  Error Queue: %d -- %s", errorValue, ERR_error_string(errorValue, NULL)));
          }
          return IO_FAILURE;
       }
    case SSL_ERROR_SYSCALL:
      if(res < 0 && (errno == EWOULDBLOCK ||
                     errno == EINTR ||
                     errno == EBUSY ||
                     errno == EAGAIN)) {
             Debug((DEBUG_DEBUG, "SSL_write returned ERROR_SYSCALL, errno %d - returning retval %d", errno, retval));
             return retval;
      }
      else {
             Debug((DEBUG_DEBUG, "SSL_write returned ERROR_SYSCALL - errno %d - returning IO_FAILURE", errno));
             return IO_FAILURE;
      }
      /*
      if(errno == EAGAIN) * its what unreal ircd does..*
      {
          Debug((DEBUG_DEBUG, "SSL_write returned ERROR_SSL - errno %d returning retval %d", errno, retval));
          return retval;
      }
      */
    case SSL_ERROR_ZERO_RETURN:
      SSL_shutdown(socket->s_ssl);
      return IO_FAILURE;
    default:
      Debug((DEBUG_DEBUG, "SSL_write return fell through - errno %d returning retval %d", errno, retval));
      return retval; /* unknown error, assume block or success*/
    }
  }
  Debug((DEBUG_DEBUG, "SSL_write return fell through(2) - errno %d returning retval %d", errno, retval));
  return retval;
}

int ssl_send(struct Client *cptr, const char *buf, unsigned int len)
{
  char fmt[16];

  if (!cli_socket(cptr).s_ssl)
    return write(cli_fd(cptr), buf, len);

  /*
   * XXX HACK
   *
   * Incomplete SSL writes must be retried with the same write buffer;
   * at this point SSL_write usually fails, so the data must be queued.
   * We're abusing the normal send queue for this.
   * Also strip \r\n from message, as sendrawto_one adds it later
   */
  ircd_snprintf(0, fmt, sizeof(fmt), "%%.%us", len - 2);
  sendrawto_one(cptr, fmt, buf);
  send_queued(cptr);
  return len;
}

int ssl_murder(void *ssl, int fd, const char *buf)
{
  if (!ssl) {
    write(fd, buf, strlen(buf));
    close(fd);
    return 0;
  }
  SSL_write((SSL *) ssl, buf, strlen(buf));
  SSL_free((SSL *) ssl);
  close(fd);
  return 0;
}

void ssl_free(struct Socket *socket)
{
  if (!socket->s_ssl)
    return;
  SSL_free(socket->s_ssl);
  --ssl_inuse;
}

int ssl_count(void)
{
  return ssl_inuse;
}

static RSA *tmp_rsa_cb(SSL *s, int export, int keylen)
{
  Debug((DEBUG_DEBUG, "Generating %d bit temporary RSA key", keylen));
  return RSA_generate_key(keylen, RSA_F4, NULL, NULL);
}

static void info_callback(const SSL *s, int where, int ret)
{
  if (where & SSL_CB_LOOP)
    Debug((DEBUG_DEBUG, "SSL state (%s): %s",
          where & SSL_ST_CONNECT ? "connect" :
          where & SSL_ST_ACCEPT ? "accept" :
          "undefined", SSL_state_string_long(s)));
  else if (where & SSL_CB_ALERT)
    Debug((DEBUG_DEBUG, "SSL alert (%s): %s: %s",
          where & SSL_CB_READ ? "read" : "write",
          SSL_alert_type_string_long(ret),
          SSL_alert_desc_string_long(ret)));
  else if (where == SSL_CB_HANDSHAKE_DONE)
    Debug((DEBUG_DEBUG, "SSL: handshake done"));
}

int sslverify_callback(int preverify_ok, X509_STORE_CTX *cert)
{
  return 1;
}

static void sslfail(char *txt)
{
  unsigned long err = ERR_get_error();
  char string[120];

  if (!err) {
    Debug((DEBUG_DEBUG, "%s: poof", txt));
  } else {
    ERR_error_string(err, string);
    Debug((DEBUG_FATAL, "%s: %s", txt, string));
    exit(2);
  }
}

void ssl_init(void)
{
  int nl;
  char pemfile[1024] = "";
  BIO *file = NULL;

  SSLeay_add_ssl_algorithms();
  SSL_load_error_strings();

  Debug((DEBUG_NOTICE, "SSL: read %d bytes of randomness", RAND_load_file("/dev/urandom", 4096)));

  ctx = SSL_CTX_new(SSLv23_method());
  SSL_CTX_set_tmp_rsa_callback(ctx, tmp_rsa_cb);
  SSL_CTX_need_tmp_RSA(ctx);
  SSL_CTX_set_mode(ctx, SSL_MODE_ENABLE_PARTIAL_WRITE | SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER);
  SSL_CTX_set_session_cache_mode(ctx, SSL_SESS_CACHE_BOTH);
  SSL_CTX_set_timeout(ctx, 300); /* XXX */
  SSL_CTX_set_info_callback(ctx, info_callback);
  SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, sslverify_callback);

  ircd_snprintf(0, pemfile, sizeof(pemfile), "%s/ircd.pem", DPATH);
  Debug((DEBUG_DEBUG, "SSL: using pem file: %s", pemfile));
  if (!SSL_CTX_use_certificate_file(ctx, pemfile, SSL_FILETYPE_PEM))
    sslfail("SSL_CTX_use_certificate_file");
  if (!SSL_CTX_use_RSAPrivateKey_file(ctx, pemfile, SSL_FILETYPE_PEM))
    sslfail("SSL_CTX_use_RSAPrivateKey_file");

  ircd_snprintf(0, pemfile, sizeof(pemfile), "%s/rsa.key", DPATH);
  Debug((DEBUG_DEBUG, "RSA: using key file: %s", pemfile));

  if (server_rsa_private_key)
  {
    RSA_free(server_rsa_private_key);
    server_rsa_private_key = NULL;
  }

  if ((file = BIO_new_file(pemfile, "r")) == NULL)
  {
    Debug((DEBUG_DEBUG, "File doesn't exist (%s)", pemfile));
    return;
  }

  server_rsa_private_key = (RSA *) PEM_read_bio_RSAPrivateKey(file, NULL,
    0, NULL);

  nl = BIO_set_close(file, BIO_CLOSE);
  BIO_free(file);

  if (!server_rsa_private_key) {
    Debug((DEBUG_DEBUG, "key invalid; check key syntax"));
  } else if (!RSA_check_key(server_rsa_private_key))
  {
    RSA_free(server_rsa_private_key);
    server_rsa_private_key = NULL;
    Debug((DEBUG_DEBUG, "invalid key, ignoring"));
  }
  else if (RSA_size(server_rsa_private_key) != 2048/8)
  {
    RSA_free(server_rsa_private_key);
    server_rsa_private_key = NULL;
    Debug((DEBUG_DEBUG, "not a 2048-bit key, ignoring"));
  }

  bio_spare_fd = save_spare_fd("SSL private key validation");
  Debug((DEBUG_DEBUG, "SSL: init ok"));
}

char *ssl_get_cipher(SSL *ssl)
{
  static char buf[400];
  int bits;
  SSL_CIPHER *c;

  buf[0] = '\0';
  strcpy(buf, SSL_get_version(ssl));
  strcat(buf, "-");
  strcat(buf, SSL_get_cipher(ssl));
  c = SSL_get_current_cipher(ssl);
  SSL_CIPHER_get_bits(c, &bits);
  strcat(buf, "-");
  strcat(buf, (char *)my_itoa(bits));
  strcat(buf, "bits");
  return (buf);
}

char *my_itoa(int i)
{
  static char buf[128];
  ircd_snprintf(0, buf, sizeof(buf), "%d", i);
  return (buf);
}

static void
binary_to_hex(unsigned char *bin, char *hex, int length)
{
  static const char trans[] = "0123456789ABCDEF";
  int i;

  for(i = 0; i < length; i++)
  {
    hex[i  << 1]      = trans[bin[i] >> 4];
    hex[(i << 1) + 1] = trans[bin[i] & 0xf];
  }

  hex[i << 1] = '\0';
}

char*
ssl_get_fingerprint(SSL *ssl)
{
  X509* cert;
  unsigned int n = 0;
  unsigned char md[EVP_MAX_MD_SIZE];
  const EVP_MD *digest = EVP_sha256();
  static char hex[BUFSIZE + 1];

  cert = SSL_get_peer_certificate(ssl);

  if (!(cert))
    return NULL;

  if (!X509_digest(cert, digest, md, &n))
  {
    X509_free(cert);
    return NULL;
  }

  binary_to_hex(md, hex, n);
  X509_free(cert);

  return (hex);
}

int
ssl_connect(struct Socket* socket)
{
  int r = 0;

  if (!socket->s_ssl) {
    socket->s_ssl = SSL_new(ctx);
    SSL_set_fd(socket->s_ssl, socket->s_fd);
  }

  r = SSL_connect(socket->s_ssl);
  if (r<=0) {
    if ((SSL_get_error(socket->s_ssl, r) == SSL_ERROR_WANT_WRITE) || (SSL_get_error(socket->s_ssl, r) == SSL_ERROR_WANT_READ))
      return 0; /* Needs to call SSL_connect() again */
    else
      return -1; /* Fatal error */
  }
  return 1; /* Connection complete */
}
#endif /* USE_SSL */
