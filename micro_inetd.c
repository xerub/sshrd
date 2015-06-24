/* micro_inetd - simple network service spawner
**
** Copyright (C)1996,2000 by Jef Poskanzer <jef@mail.acme.com>.
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
** ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
** IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
** ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
** FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
** DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
** OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
** HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
** LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
** OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
** SUCH DAMAGE.
*/

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <memory.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#if defined(AF_INET6) && defined(IN6_IS_ADDR_V4MAPPED)
#undef USE_IPV6
#endif


static void usage( void );
static int initialize_listen_socket( int pf, int af, unsigned short port );
static void child_handler( int sig );


static char* argv0;


int
main2( int argc, char* argv[] )
    {
    unsigned short port;
    char** child_argv;
    int listen_fd, conn_fd;
    struct sockaddr_in sa_in;
    socklen_t sz;

    argv0 = argv[0];

    /* Get arguments. */
    if ( argc < 3 )
	usage();
    port = (unsigned short) atoi( argv[1] );
    child_argv = argv + 2;

    /* Initialize listen socket.  If we have v6 use that, since its sockets
    ** will accept v4 connections too.  Otherwise just use v4.
    */
#ifdef USE_IPV6
    listen_fd = initialize_listen_socket( PF_INET6, AF_INET6, port );
#else /* USE_IPV6 */
    listen_fd = initialize_listen_socket( PF_INET, AF_INET, port );
#endif /* USE_IPV6 */

    /* Set up a signal handler for child reaping. */
    (void) signal( SIGCHLD, child_handler );

    for (;;)
	{
	/* Accept a new connection. */
	sz = sizeof(sa_in);
	conn_fd = accept( listen_fd, (struct sockaddr*) &sa_in, &sz );
	if ( conn_fd < 0 )
	    {
	    if ( errno == EINTR )	/* because of SIGCHLD (or ptrace) */
		continue;
	    perror( "accept" );
	    exit( 1 );
	    }

	/* Fork a sub-process. */
	if ( fork() == 0 )
	    {
	    /* Close standard descriptors and the listen socket. */
	    (void) close( 0 );
	    (void) close( 1 );
	    (void) close( 2 );
	    (void) close( listen_fd );
	    /* Dup the connection onto the standard descriptors. */
	    (void) dup2( conn_fd, 0 );
	    (void) dup2( conn_fd, 1 );
	    (void) dup2( conn_fd, 2 );
	    (void) close( conn_fd );
	    /* Run the program. */
	    (void) execv( child_argv[0], child_argv );
	    /* Something went wrong. */
	    perror( "execl" );
	    exit( 1 );
	    }
	/* Parent process. */
	(void) close( conn_fd );
	}

    }


static void
usage( void )
    {
    (void) fprintf( stderr, "usage:  %s port program [args...]\n", argv0 );
    exit( 1 );
    }


static void
child_handler( int sig )
    {
    pid_t pid;
    int status;

    /* Set up the signal handler again.  Don't need to do this on BSD
    ** systems, but it doesn't hurt.
    */
    (void) signal( SIGCHLD, child_handler );

    /* Reap defunct children until there aren't any more. */
    for (;;)
        {
        pid = waitpid( (pid_t) -1, &status, WNOHANG );
        if ( (int) pid == 0 )           /* none left */
            break;
        if ( (int) pid < 0 )
            {
            if ( errno == EINTR )       /* because of ptrace */
                continue;
            /* ECHILD shouldn't happen with the WNOHANG option, but with
            ** some kernels it does anyway.  Ignore it.
            */
            if ( errno != ECHILD )
                perror( "waitpid" );
            break;
            }
        }
    }


static int
initialize_listen_socket( int pf, int af, unsigned short port )
    {
    int listen_fd;
    int on;
#ifdef USE_IPV6
    struct sockaddr_in6 sa_in;
#else /* USE_IPV6 */
    struct sockaddr_in sa_in;
#endif /* USE_IPV6 */

    /* Create socket. */
    listen_fd = socket( pf, SOCK_STREAM, 0 );
    if ( listen_fd < 0 )
        {
	perror( "socket" );
        exit( 1 );
        }

    /* Allow reuse of local addresses. */
    on = 1;
    if ( setsockopt(
             listen_fd, SOL_SOCKET, SO_REUSEADDR, (char*) &on, sizeof(on) ) < 0 )
	{
	perror( "setsockopt SO_REUSEADDR" );
	exit( 1 );
	}

    /* Set up the sockaddr. */
    (void) memset( (char*) &sa_in, 0, sizeof(sa_in) );
#ifdef USE_IPV6
    sa_in.sin6_family = af;
    sa_in.sin6_addr = in6addr_any;
    sa_in.sin6_port = htons( port );
#else /* USE_IPV6 */
    sa_in.sin_family = af;
    sa_in.sin_addr.s_addr = htonl( INADDR_ANY );
    sa_in.sin_port = htons( port );
#endif /* USE_IPV6 */

    /* Bind it to the socket. */
    if ( bind( listen_fd, (struct sockaddr*) &sa_in, sizeof(sa_in) ) < 0 )
        {
	perror( "bind" );
        exit( 1 );
        }

    /* Start a listen going. */
    if ( listen( listen_fd, 1024 ) < 0 )
        {
	perror( "listen" );
        exit( 1 );
        }

    return listen_fd;
    }
