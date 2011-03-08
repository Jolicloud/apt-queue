#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#define DPKG_LOCK "/var/lib/dpkg/lock"
#define LOG_FILE  "/var/log/apt-queue"
#define TIME_SIZE 256
#define MAX_ATTEMPTS 5


void print_time_msg( char* msg )
{
    time_t     cur;
    struct tm* loc;
    char       str[ TIME_SIZE ];

    cur = time( NULL );
    loc = localtime( &cur );
    strftime( str, TIME_SIZE, "%F %T %z", loc );

    printf( ">> %s: %s\n", str, msg );
}

static int get_lock()
{
    char* lockFile = DPKG_LOCK;
    int lockH;
    int err = 0;

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

    return err;
}

static int process_cmd( int argc, char** argv )
{
    size_t length = 0;
    int err = 0;
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
        setenv( "DEBIAN_FRONTEND", "noninteractive", 1 );
        err = system( cmd );

        free( cmd );
    }

    return err;
}

int main( int argc, char** argv )
{
    pid_t cpid;
    int err = 0;
    int attempt = 0;

    // Ignore SIGHUP, gdebi likes to send it after installing an
    // application when its postinst is complete, but this negates the
    // purpose of queueing in the background.
    signal( SIGHUP, SIG_IGN );

    cpid = fork();

    if ( cpid == -1 ) {
        perror( "fork" );
        exit( EXIT_FAILURE );
    }
    else if ( cpid > 0 ) {
        printf( "Backgrounding process, child PID: %d\n", cpid );
        printf( "Logging to %s\n", LOG_FILE );
        exit( EXIT_SUCCESS );
    }

    // Ensure a new session group parent if caller process dies before
    // apt-queue establishes the lock.
    setsid();

    // Forward all STDOUT/STDERR data to the log file, this is useful to
    // log the system() call for later in a separate file
    if ( freopen( LOG_FILE, "a", stdout ) == NULL ) {
        printf( "Warning: Failed to freopen(3) stdout" );
    }
    if ( freopen( LOG_FILE, "a", stderr ) == NULL ) {
        printf( "Warning: Failed to freopen(3) stderr" );
    }

    print_time_msg( "Starting Program" );

    for ( attempt = 0; attempt < MAX_ATTEMPTS; attempt++ ) {
        // This get_lock() function waits indefinitely for a successful
        // lock if it encounters a temporary failure (lock file is busy).
        // If it's a permanent failure (lock is unlockable) it returns
        // non-zero and is retried in this main loop up to MAX_ATTEMPTS times.
        err = get_lock();
        if ( err == 0 ) {
            err = process_cmd( argc, argv );
            // If err is 0, the queued command has executed cleanly.
            if ( err == 0 )
                break;
        }
        printf( ">> got err: %d .. attempt %d of %d\n", err, attempt + 1,
                MAX_ATTEMPTS );
    }

    printf( ">> return err: %d\n", err );
    print_time_msg( "Program Finished" );

    return err;
}


// vim:et:ai:ts=4:sw=4
