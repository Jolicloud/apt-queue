#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#define DPKG_LOCK "/var/lib/dpkg/lock"

int main( int argc, char** argv )
{
    pid_t cpid;
    char* lockFile = DPKG_LOCK;
    int lockH;
    int err;

    cpid = fork();

    if ( cpid == -1 ) {
        perror( "fork" );
        exit( EXIT_FAILURE );
    }
    else if ( cpid > 0 ) {
        printf( "Backgrounding process, child PID: %d\n", cpid );
        exit( EXIT_SUCCESS );
    }

    freopen( "/var/log/apt-queue.log", "a", stdout );

    lockH = open( lockFile, O_RDWR );
    if ( lockH == -1 ) {
        printf( "Couldn't open: %s: %d, %d\n", DPKG_LOCK, lockH, errno );
        printf( "Are you root?\n" );
        err = -1;
    }
    else {
        struct flock fl;

        // Use the exact same lock request APT and dpkg are establishing.
        fl.l_type = F_WRLCK;
        fl.l_whence = SEEK_SET;
        fl.l_start = 0;
        fl.l_len = 0;

        // Try to set the file lock, sleep+loop if the error is EAGAIN,
        // this means the resource was temporarially unavailable due to
        // another running APT process.
        do {
            err = fcntl( lockH, F_SETLK, &fl );

            if ( ! err ) {
                printf( "Got a lock!\n" );
                break;
            }

            printf( "Could not get a lock %s - ", lockFile );
            if ( errno == EAGAIN ) {
                printf( "Resource not available... sleeping\n" );
                sleep( 1 );
            }
            else {
                if ( errno == EACCES ) {
                    printf( "Permission Denied\n" );
                }
                else {
                    printf( "Unknown Error?: %d\n", errno );
                }
                // Set err to errno so we can use it as the program's
                // return value.
                err = errno;
            }
        } while ( err == -1 && errno == EAGAIN );
    }
    close( lockH );


    // OK, if err is zero, we're good to go. Run the command!
    if ( err == 0 ) {
        size_t length = 0;
        int i;

        // Identify the length of the queued command for memory allocation
        for ( i = 1; i < argc; i++ )
            length += strlen( argv[ i ] ) + 1;

        if ( length == 0 ) {
            printf( "Nothing to run?\n" );
        }
        else {
            char* cmd;

            cmd = malloc( length );
            length = 0;

            for ( i = 1; i < argc; i++ ) {
                strcpy( cmd + length, argv[ i ] );
                length += strlen( argv[ i ] );
                strcpy( cmd + length, " " );
                length++;
            }

            printf( "Running Queued Command: %s\n", cmd );
            printf( "-------------------------------------------\n", cmd );

            fflush( NULL );
            err = system( cmd );

            free( cmd );
        }
    }
    return err;
}


// vim:et:ai:ts=4
