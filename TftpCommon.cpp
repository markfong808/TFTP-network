//
// Created by B Pan on 1/15/24.
//

#include <cstdio>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <cstdlib>
#include <csignal>
#include <chrono>
#include <thread>
#include "TftpError.h"
#include "TftpOpcode.h"
#include "TftpConstant.h"

// To track how retransmit/retry has occurred.
static int retryCount = 0;

// Helper function to print the first len bytes of the buffer in Hex
static void printBuffer(const char * buffer, unsigned int len) {
    for(int i = 0; i < len; i++) {
        printf("%x,", buffer[i]);
    }
    printf("\n");
}

// increment retry count when timeout occurs. 
static void handleTimeout(int signum ){
    retryCount++;
    printf("timeout occurred! count %d\n", retryCount);
}

static int registerTimeoutHandler( ){
    signal(SIGALRM, handleTimeout);

    /* disable the restart of system call on signal. otherwise the OS will be stuck in
     * the system call
     */
    if( siginterrupt( SIGALRM, 1 ) == -1 ){
        printf( "invalid sig number.\n" );
        return -1;
    }
    return 0;
}

/*
 * Useful things:
 * alarm(1) // set timer for 1 sec
 * alarm(0) // clear timer
 * std::this_thread::sleep_for(std::chrono::milliseconds(200)); // slow down transmission
 */


/*
 * TODO: Add common code that is shared between your server and your client here. For example: helper functions for
 * sending bytes, receiving bytes, parse opcode from a tftp packet, parse data block/ack number from a tftp packet,
 * create a data block/ack packet, and the common "process the file transfer" logic.
 */
