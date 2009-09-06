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
    char* lockFile = DPKG_LOCK;
    int lockH;
    int err;

    lockH = open( lockFile, O_RDWR );
    if ( lockH == -1 ) {
        printf( "Couldn't open: %s: %d, %d\n", DPKG_LOCK, lockH, errno );
        printf( "Are you root?\n" );
        err = -1;
    }
    else {
        struct flock fl;

        fl.l_type = F_WRLCK;
        fl.l_whence = SEEK_SET;
        fl.l_start = 0;
        fl.l_len = 0;

        do {
            err = fcntl( lockH, F_SETLK, &fl );

            if ( ! err ) {
                printf( "Got a lock!\n" );
                break;
            }

            printf( "Could not get a lock %s - ", lockFile );
            if ( errno == EACCES ) {
                printf( "Permission Denied\n" );
                // Stop the loop here, force err to no longer be -1
                err = errno;
            }
            else if ( errno == EAGAIN ) {
                printf( "Resource not available\n" );
            }
            else {
                printf( "Error: %d\n", errno );
            }
            sleep( 1 );
        } while ( err == -1 );
    }

    close( lockH );

    if ( err == 0 ) {
        size_t length = 0;
        int i;

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
            err = system( cmd );

            free( cmd );
        }
    }
    return err;
}


// vim:et:ai:ts=4
