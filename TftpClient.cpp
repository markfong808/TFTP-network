//
// Created by B Pan on 12/3/23.
//

//TFTP client program - CSS 432 - Winter 2024

#include <cstdio>
#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <fstream>
#include "TftpError.h"
#include "TftpOpcode.h"
#include "TftpCommon.cpp"

#define SERV_UDP_PORT 61125
#define SERV_HOST_ADDR "127.0.0.1"

/* A pointer to the name of this program for error reporting.      */
char *program;

/* The main program sets up the local socket for communication     */
/* and the server's address and port (well-known numbers) before   */
/* calling the processFileTransfer main loop.                      */

int main(int argc, char *argv[]) {
    program = argv[0];

    int sockfd;
    struct sockaddr_in cli_addr, serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    memset(&cli_addr, 0, sizeof(cli_addr));
    socklen_t serv_addrlen = sizeof(serv_addr);

    /*
     * TODO: initialize server and client address, create socket and bind socket as you did in
     * programming assignment 1
     */
    // 1.Fill in serv_addr
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(SERV_UDP_PORT);             //
    serv_addr.sin_addr.s_addr = inet_addr(SERV_HOST_ADDR); //

    // 2.Fill in cli_addr
    cli_addr.sin_family = AF_INET;
    cli_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    cli_addr.sin_port = htons(0);

    // 3. Create a UDP socket
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }

    // 4.bind
    if (bind(sockfd, (struct sockaddr *)&cli_addr, sizeof(cli_addr)) < 0)
    {
        perror("Error binding UDP socket");
        close(sockfd); // Close the socket before exiting
        exit(EXIT_FAILURE);
    }
    else
    {
        std::cout << "Bind socket successful\n"
                  << std::endl;
    }

    /*
     * TODO: Verify arguments, parse arguments to see if it is a read request (r) or write request (w),
     * parse the filename to transfer, open the file for read or write
     */
    // Verify arguments
    if (argc != 3)
    {
        fprintf(stderr, "Usage: %s <r | w> <filename>\n", argv[0]);
        exit(1);
    }
    // parse the filename to transfer, open the file for read or write
    char *request = argv[1];
    char *filename = argv[2];

    /*
     * TODO: create the 1st tftp request packet (RRQ or WRQ) and send it to the server via socket.
     * Remember to use htons when filling the opcode in the tftp request packet.
     */
    if (strcmp(request, "r") == 0)
    {
        char buffer[MAX_PACKET_LEN];                    // tftp request packet (RRQ or WRQ)
        char *bpt = buffer;                             // point to the beginning of the buffer
        unsigned short *opCode = (unsigned short *)bpt; // opCode points to the beginning of the buffer
        *opCode = htons(TFTP_RRQ);                      // fill in op code for packet.
        bpt += 2;
        // unsigned short *filename = (unsigned short *)(bpt + 2);
        char requested_filename[255];
        size_t filenameLength = strlen(filename);
        // std::cout << "Requested filename: " << filename << std::endl;
        strcpy(requested_filename, filename); // put the requested file name into the request buffer
        // std::cout << "Filename length: " << filenameLength << std::endl;
        strncpy(bpt, requested_filename, filenameLength);
        bpt[filenameLength] = '\0'; // null terminator to mark the end
        bpt += filenameLength;
        // printBuffer(buffer,MAX_PACKET_LEN);
        bpt += 1;
        const char *mode = "octet";
        char modeArray[10];
        size_t modeLength = strlen(mode);
        strcpy(modeArray, mode);
        strncpy(bpt, modeArray, modeLength);
        bpt += strlen(mode) + 1; // Add 1 for the null terminator
        bpt[modeLength] = '\0';  // null terminator to mark the end

        std::cout << "The request packet is:" << std::endl;
        // printBuffer(buffer, MAX_PACKET_LEN);

        // Send Read Request to tftp server
        ssize_t num_bytes_sent = sendto(sockfd, buffer, sizeof(buffer), 0, (struct sockaddr *)&serv_addr, sizeof(serv_addr));

        if (num_bytes_sent < 0)
        {
            perror("Error sending request packet");
            close(sockfd);
            exit(EXIT_FAILURE);
        }
    }
    else if (strcmp(request, "w") == 0)
    {                                                   // Send Write Request to tftp server
        char buffer[MAX_PACKET_LEN];                    // tftp request packet (RRQ or WRQ)
        char *bpt = buffer;                             // point to the beginning of the buffer
        unsigned short *opCode = (unsigned short *)bpt; // opCode points to the beginning of the buffer
        *opCode = htons(TFTP_WRQ);                      // fill in op code for packet.
        bpt += 2;
        char sent_filename[255];
        size_t filenameLength = strlen(filename);
        // std::cout << "Requested filename: " << filename << std::endl;
        strcpy(sent_filename, filename); // put the requested file name into the request buffer
        // std::cout << "Filename length: " << filenameLength << std::endl;
        strncpy(bpt, sent_filename, filenameLength);
        bpt[filenameLength] = '\0'; // null terminator to mark the end
        bpt += filenameLength;
        // printBuffer(buffer,MAX_PACKET_LEN);
        bpt += 1;
        const char *mode = "octet";
        char modeArray[10];
        size_t modeLength = strlen(mode);
        strcpy(modeArray, mode);
        strncpy(bpt, modeArray, modeLength);
        bpt += strlen(mode) + 1; // Add 1 for the null terminator
        bpt[modeLength] = '\0';  // null terminator to mark the end

        // send WRQ to server
        ssize_t num_bytes_sent = sendto(sockfd, buffer, sizeof(buffer), 0, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
    }
    /*
     * TODO: process the file transfer
     */
    std::cout << "Processing tftp request..." << std::endl;

#define MAX_DATA_SIZE 512
    bool last_packet_received = false;
    bool last_packet_sent = false;
    char receiver[MAX_PACKET_LEN]; // receive to store the incoming data
    char Ack_receiver[TFTP_ACK];

    if (*request == 'w')
    { // WRQ

        size_t BytesReceived = recvfrom(sockfd, receiver, MAX_PACKET_LEN, 0, (struct sockaddr *)&serv_addr, &serv_addrlen);

        int block = parseBlockNumber(receiver);

        std::cout
            << "Received Ack "
            << "#" << block << std::endl;

        block++;

        std::string filePath = std::string(CLIENT_FOLDER) + std::string(filename);
        // std::cout << "Full path: " << filePath << std::endl;
        std::ifstream file(filePath, std::ios::binary);
        if (!file)
        { // check to see if the file can be open first before sending
            perror("Error opening file for read");
        }

        int packet = 1;
        // unsigned short blockNumber = 1;
        char Data_buffer[MAX_PACKET_LEN];

        while (true)
        {
            file.read(Data_buffer + 4, MAX_PACKET_LEN - 4);
            size_t bytesRead = file.gcount();
            char *pos = Data_buffer;
            pos += 4;
            std::cout << "Requested File chunk size " << bytesRead << std::endl; // 7465 bytes for large.txt
            // std::cout << "Creating Packet No. " << packet++ << std::endl;
            createDataPacket(Data_buffer, block, bytesRead, pos);
            // printBuffer(Data_buffer, MAX_PACKET_LEN);
            size_t BytesSend = sendto(sockfd, Data_buffer, 4 + bytesRead, 0, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
            pos += bytesRead;
            block++;

            // listen for ack check
            char Rec_Ackbuffer[TFTP_ACK];
            recvfrom(sockfd, Rec_Ackbuffer, sizeof(Rec_Ackbuffer), 0, (struct sockaddr *)&serv_addr, &serv_addrlen);
            // printBuffer(Rec_Ackbuffer, TFTP_ACK);
            int block = parseBlockNumber(Rec_Ackbuffer);

            std::cout
                << "Received Ack "
                << "#" << block << std::endl;

            if (file.eof())
            {
                last_packet_sent = true;
                break;
            }
        } // while loop end
    }
    else if (*request == 'r')
    { // RRQ
        size_t BytesReceived = 0;
        for (;;)
        {
            size_t BytesReceived = recvfrom(sockfd, receiver, MAX_PACKET_LEN, 0, (struct sockaddr *)&serv_addr, &serv_addrlen);
            std::cout << "Bytes_received: " << BytesReceived << std::endl;
            // printBuffer(receiver, MAX_PACKET_LEN);
            uint16_t opcode = parseOpcode(receiver);
            // std::cout << "Optcode " << opcode << std::endl;

            // check the opcode
            if (opcode == TFTP_DATA)
            {
                int block = parseBlockNumber(receiver);
                std::cout
                    << "Received block "
                    << "#" << block << std::endl;
                printBuffer(receiver, MAX_PACKET_LEN);

                std::string filePath = std::string(CLIENT_FOLDER) + std::string(filename);
                writeFile(filePath, receiver, BytesReceived);

                // send ACK check for received block
                char Ackbuffer[TFTP_ACK];
                createAckPacket(opcode, block, Ackbuffer);
                // printBuffer(Ackbuffer,TFTP_ACK);
                sendto(sockfd, Ackbuffer, sizeof(Ackbuffer), 0, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
            }

            if (BytesReceived - 4 < MAX_DATA_SIZE)
            {
                last_packet_received = true;
                std::cout << "This is the last packet." << std::endl; // detect last packet
            }

            std::cout << "Bytesafter." << BytesReceived << std::endl;
            std::cout << "lastpack bool: " << std::boolalpha << last_packet_received << std::endl;

            if (last_packet_received)
            {

                break;
            }
        }
    } // else statement end

    //  if (last_packet_sent)
    // {
    //    break;
    //  }
    /*
     * TODO: Don't forget to close any file that was opened for read/write, close the socket, free any
     * dynamically allocated memory, and necessary clean up.
     */
    close(sockfd);

    exit(0);
} // main end
