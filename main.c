#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#include <openssl/rand.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

// Simple structure to keep track of the handle, and
// of what needs to be freed later.
typedef struct {
    int socket;
    SSL *sslHandle;
    SSL_CTX *sslContext;
} connection;

// Just to avoid using ip/port everywhere
#define SERVER  "127.0.0.1"
#define PORT 8443

// Establish a regular tcp connection
int tcpConnect ()
{
    int error, handle;
    struct hostent *host;
    struct sockaddr_in server;

    host = gethostbyname (SERVER);
    handle = socket (AF_INET, SOCK_STREAM, 0);
    if (handle == -1)
    {
        perror ("Socket");
        handle = 0;
    }
    else
    {
        server.sin_family = AF_INET;
        server.sin_port = htons (PORT);
        server.sin_addr = *((struct in_addr *) host->h_addr);
        bzero (&(server.sin_zero), 8);

        error = connect (handle, (struct sockaddr *) &server,
                         sizeof (struct sockaddr));
        if (error == -1)
        {
            perror ("Connect");
            handle = 0;
        }
    }

    return handle;
}

// Establish a connection using an SSL layer
connection *sslConnect (void)
{
    connection *c;

    c = malloc (sizeof (connection));
    c->sslHandle = NULL;
    c->sslContext = NULL;

    c->socket = tcpConnect ();
    if (c->socket)
    {
        SSL_load_error_strings ();
        SSL_library_init ();

        // New context saying we are a client, and using SSL 2 or 3
        c->sslContext = SSL_CTX_new (SSLv23_client_method ());
        if (c->sslContext == NULL)
            ERR_print_errors_fp (stderr);

        c->sslHandle = SSL_new (c->sslContext);
        if (c->sslHandle == NULL)
            ERR_print_errors_fp (stderr);

        if (!SSL_set_fd (c->sslHandle, c->socket))
            ERR_print_errors_fp (stderr);

        if (SSL_connect (c->sslHandle) != 1)
            ERR_print_errors_fp (stderr);
    }
    else
    {
        perror ("Connect failed");
    }

    return c;
}

// Disconnect & free connection struct
void sslDisconnect (connection *c)
{
    if (c->socket)
        close (c->socket);
    if (c->sslHandle)
    {
        SSL_shutdown (c->sslHandle);
        SSL_free (c->sslHandle);
    }
    if (c->sslContext)
        SSL_CTX_free (c->sslContext);

    free (c);
}

// Read all available text from the connection
char *sslRead (connection *c)
{
    const int readSize = 10240;
    char *rc = NULL;
    int received, count = 0;
    char buffer[10240];

    if (c)
    {
        while (1)
        {
            if (!rc)
                rc = malloc (readSize * sizeof (char) + 1);
            else
                rc = realloc (rc, (count + 1) *
                                  readSize * sizeof (char) + 1);

            received = SSL_read (c->sslHandle, buffer, readSize);
            buffer[received] = '\0';
            printf ("Received Buffer (Size %d): %s\n", received, buffer);

            if (received > 0)
                strcat (rc, buffer);

            if (received < readSize)
                break;
            count++;
        }
    }

    return rc;
}

// Write text to the connection
void sslWrite (connection *c, char *text)
{
    if (c)
        SSL_write (c->sslHandle, text, strlen (text));
}

int main (int argc, char **argv)
{
    connection *c;
    char *response;

    c = sslConnect ();

    int flag = 1;
    int result = setsockopt(c->socket, IPPROTO_TCP, TCP_NODELAY, (char *) &flag, sizeof(int));

    sslWrite(c, "Hello\n");

    //SSL_heartbeat(c->sslHandle);
    SSL_heartbeat(c->sslHandle);

    sslWrite(c, "Hello After heartbeat\n");
    response = sslRead(c);
    response = sslRead(c);


//  printf ("%s\n", response);

    sslDisconnect (c);
    free (response);

    return 0;
}
