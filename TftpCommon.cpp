#include <cstring>
#include <iostream>
#include <fstream>
#include <string>
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
// get the file bytes
static size_t getfilesize(const char *filename)
{
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open())
    {
        // std::cerr << "Error opening file!" << std::endl;
        return -1;
    }
    file.seekg(0, std::ios::end);
    long filesize = file.tellg();
    file.seekg(0, std::ios::beg);

    file.close();

    return filesize;
}

// helper function to parse opcode from the packet
static uint16_t parseOpcode(const char *buffer)
{
    uint16_t opcode;
    memcpy(&opcode, buffer, sizeof(uint16_t));
    opcode = ntohs(opcode); // Convert from network byte order to host byte order
    // std::cout << "Opcode: " << opcode << std::endl;
    return opcode;
}


static std::string parseErrMsg(const char (&buffer)[MAX_PACKET_LEN], const int &num_bytes_recv)
{
    return std::string(buffer + 4, num_bytes_recv - 4);
}

    // // Helper function to parse data block number from a TFTP data packet
    static int parseBlockNumber(const char *buffer)
    {
        // Parse the blocknumber from the Data packet
        unsigned short block;
        memcpy(&block, &buffer[2], sizeof(unsigned short)); // Extract block number
        block = ntohs(block);

        return block;
    }

    // Helper function to parse Ack number from a TFTP ACK packet
    static int parseAckNumber(const char *buffer)
    {
        // Parse the blocknumber from the Data packet
        unsigned short ack;
        memcpy(&ack, &buffer[2], sizeof(unsigned short)); // Extract ack num
        ack = ntohs(ack);

        return ack;
    }

    // Helper function to parse Ack number from a TFTP Error packet
    static int parseErrorCode(const char *buffer)
    {
        // Parse the errorcode from the Data packet
        unsigned short error;
        memcpy(&error, &buffer[2], sizeof(unsigned short)); // Extract error code
        error = ntohs(error);

        return error;
    }
    // Helper function to create a TFTP Data packet
    static void createDataPacket(char *buffer, unsigned short blockNumber, size_t bytes, char *Pos)
    {

        char *bpt = buffer;                                  // point to the beginning of the buffer
        unsigned short *opCode = (unsigned short *)bpt;      // opCode points to the beginning of the buffer
        *opCode = htons(TFTP_DATA);                          // fill in op code for packet.
        unsigned short *block = (unsigned short *)(bpt + 2); // move bpt towards right by 2 bytes
        *block = htons(blockNumber);                         // fill in block number
        bpt = bpt + 4;                                       // pointer to the beginning of the actual file data
        std::memcpy(bpt, Pos, bytes);
        bpt += bytes;
    }

    // // Helper function to create a TFTP ACK packet
    static void createAckPacket(uint16_t opcode, int blockNumber, char *buffer)
    {
        char *bpt = buffer;
        unsigned short *opCode = (unsigned short *)bpt;
        *opCode = htons(TFTP_ACK); // Change the Opcode as ACK type
        bpt += 2;                  // now points to block field
        unsigned short *block = (unsigned short *)bpt;
        *block = htons(blockNumber);
        opcode = TFTP_ACK;
    }

    static void writeFile(const std::string &filename, char(&filebuffer)[MAX_PACKET_LEN], const int &num_bytes_recv)
    {
        std::ofstream outputFile(filename, std::ios::app | std::ios::out | std::ios::binary);
        outputFile.write(filebuffer + 4, num_bytes_recv - 4);
        outputFile.close();
    }

    // Helper function to create a TFTP Error packet
    static void createErrorPacket(char *buffer, unsigned short errorcode, size_t bytes)
    {
        char *bpt = buffer;
        unsigned short *opCode = (unsigned short *)bpt;
        *opCode = htons(TFTP_ERROR);
        unsigned short *ErrorCode = (unsigned short *)(bpt + 2);
        *ErrorCode = htons(errorcode);
        bpt = bpt + 4;

        std::memcpy(bpt, buffer + 4, bytes - 1);
        bpt += strlen(buffer) + 1;
    }


