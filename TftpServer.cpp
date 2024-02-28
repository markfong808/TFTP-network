//
// Created by B Pan on 12/3/23.
//
//for system calls, please refer to the MAN pages help in Linux
//TFTP server program over udp - CSS432 - winter 2024
#include <cstdio>
#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <algorithm>
#include <filesystem>
#include "TftpCommon.cpp"

#define SERV_UDP_PORT 61125

char *program;

int handleIncomingRequest(int sockfd) {

    struct sockaddr cli_addr;
    std::cout << "Waiting to receive request" << std::endl;

    /*
     * TODO: define necessary variables needed for handling incoming requests.
     */
    socklen_t addrlen = sizeof(cli_addr);
    char buffer[MAX_PACKET_LEN]; // Buffer to store incoming data
    bool last_packet = false;

    for (;;) {

        /*
         * TODO: Receive the 1st request packet from the client
         */
        ssize_t num_bytes = recvfrom(sockfd, buffer, sizeof(buffer), 0, (struct sockaddr *)&cli_addr, &addrlen);
        // printBuffer(buffer, sizeof(buffer));

        if (num_bytes < 0) // check for error
        {
            perror("recvfrom");
            return -1;
        }

        /*
         * TODO: Parse the request packet. Based on whether it is RRQ/WRQ, open file for read/write.
         * Create the 1st response packet, send it to the client.
         */
        uint16_t opcode = parseOpcode(buffer);
        // std::cout << "Opcode: " << opcode << std::endl;

        // Check if it's an RRQ or WRQ
        if (opcode == TFTP_RRQ)
        {

            // std::cout << "Confirme received Read Request (RRQ)\n" << std::endl;

            // Extract filename and mode from the packet
            char filename[255];
            char mode[10];
            strncpy(filename, buffer + 2, 255);
            strncpy(mode, buffer + 2 + strlen(filename) + 1, 10);

            // Print the extracted filename and mode
            std::cout << "Requested filename is: " << filename << std::endl;
            //std::cout << "Mode: " << mode << std::endl;            
            
            int packet = 1;
            unsigned short blockNumber = 1;
            char Data_buffer[MAX_PACKET_LEN];
            // creating full path
            std::string filePath = std::string(SERVER_FOLDER) + std::string(filename);
            //std::cout << "Full path: " << filePath << std::endl;            
            std::ifstream file(filePath, std::ios::binary);

            while (true)
            {
                file.read(Data_buffer + 4, MAX_PACKET_LEN - 4);
                size_t bytesRead = file.gcount();
                char *pos = Data_buffer;
                pos += 4;
                std::cout << "Requested File chunk size " << bytesRead << std::endl; // 7465 bytes for large.txt
                std::cout << "Creating Packet No. " << packet++ << std::endl;
                createDataPacket(Data_buffer, blockNumber, bytesRead, pos);
                //printBuffer(Data_buffer, MAX_PACKET_LEN);
                size_t BytesSend = sendto(sockfd, Data_buffer, 4 + bytesRead, 0, (struct sockaddr *)&cli_addr, sizeof(cli_addr));
                pos += bytesRead;
                blockNumber++;

                if (file.eof())
                {
                    break;
                }
            }
        }
        else if (opcode == TFTP_WRQ)
        {
            char filename[255];
            char mode[10];
            strncpy(filename, buffer + 2, 255);
            strncpy(mode, buffer + 2 + strlen(filename) + 1, 10);
            std::cout << "Requested filename is: " << filename << std::endl;
            //  Check the filename first to see if it is already existed in the path
            std::string filePath = std::string(SERVER_FOLDER) + std::string(filename);
            std::cout << "Full path: " << filePath << std::endl;
            
            char ErrorMsgbuffer[MAX_PACKET_LEN];
            unsigned short errorcode = 6;
            bool error = false;
            if(access(filePath.c_str(),F_OK) != -1){
                // File already exists, handle accordingly
                std::cout << "File already exists!" << std::endl;
                const char *errorMessage = "File already exists: ";
                error = true;
                size_t errorMsgLength = strlen(errorMessage);
                const size_t filePathLength = filePath.size();
                const size_t totalLength = errorMsgLength + filePathLength + 1;
                strncpy(ErrorMsgbuffer + 4 ,errorMessage,errorMsgLength);
                strncpy(ErrorMsgbuffer + 4 + errorMsgLength + 1, filePath.c_str(),filePathLength);
                ErrorMsgbuffer[4 + errorMsgLength + filePathLength + 1] = '\0'; // Null-terminate the string
                createErrorPacket(ErrorMsgbuffer,errorcode,totalLength);
                size_t BytesSend = sendto(sockfd, ErrorMsgbuffer, 4 + totalLength + 1, 0, (struct sockaddr *)&cli_addr, sizeof(cli_addr));
                // printBuffer(ErrorMsgbuffer, MAX_PACKET_LEN);
                // std::string errMsg = parseErrMsg(ErrorMsgbuffer, BytesSend);
                // std::cout << "Error msg: " << errMsg << std::endl;
            } else { //Proceed to receive
             
            // send ack
            int Ack = 0;
            // char Ackbuffer[TFTP_ACK];
            createAckPacket(opcode, Ack, buffer);
            // printBuffer(buffer,TFTP_ACK);
            sendto(sockfd, buffer, sizeof(buffer), 0, (struct sockaddr *)&cli_addr, sizeof(cli_addr));

            bool last_packet_received = false;
            char receiver[MAX_PACKET_LEN]; // receive to store the incoming data
            #define MAX_DATA_SIZE 512
            size_t BytesReceived = 0;
            for (;;)
            {
                do
                { // start receiving from client
                    size_t BytesReceived = recvfrom(sockfd, receiver, MAX_PACKET_LEN, 0, (struct sockaddr *)&cli_addr, &addrlen);
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
                        // printBuffer(receiver, MAX_PACKET_LEN);

                        std::string filePath = std::string(SERVER_FOLDER) + std::string(filename);
                        writeFile(filePath, receiver, BytesReceived);

                        // send ACK check for received block
                        char Ackbuffer[TFTP_ACK];
                        createAckPacket(opcode, block, Ackbuffer);
                        // printBuffer(Ackbuffer,TFTP_ACK);
                        sendto(sockfd, Ackbuffer, sizeof(Ackbuffer), 0, (struct sockaddr *)&cli_addr, sizeof(cli_addr));
                    }

                    if (BytesReceived - 4 < MAX_DATA_SIZE)
                    {
                        last_packet_received = true;
                        std::cout << "This is the last packet." << std::endl; // detect last packet
                    }
                } while (BytesReceived == MAX_PACKET_LEN);

                if (last_packet_received)
                {
                    break;
                }
            } //infinite loop end
          } //else statement end
        }
        else if (opcode == TFTP_ACK)
        {
            int block = parseBlockNumber(buffer);

            std::cout
                << "Received Ack "
                << "#" << block << std::endl;
        }
        else
        {
            std::cout << "Invalid opCode" << TFTP_ERROR_INVALID_OPCODE << std::endl;
            uint16_t opcode;
            memcpy(&opcode, buffer, sizeof(uint16_t));
            opcode = ntohs(opcode); // Convert from network byte order to host byte order
            printf("Received opcode: %d\n", opcode);
        }
        /*
         * TODO: process the file transfer
         */

        /*
         * TODO: Don't forget to close any file that was opened for read/write, close the socket, free any
         * dynamically allocated memory, and necessary clean up.
         */
    } // big infinite forloop end
} // handle incoming request func end

int main(int argc, char *argv[]) {
    program=argv[0];

    int sockfd;
    struct sockaddr_in serv_addr;

    memset(&serv_addr, 0, sizeof(serv_addr));

    /*
     * TODO: initialize the server address, create socket and bind the socket as you did in programming assignment 1
     */
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }
    serv_addr.sin_family = AF_INET;            // Set address family to IPv4
    serv_addr.sin_port = htons(SERV_UDP_PORT); // Set the UDP port number (convert to network byte order)
    serv_addr.sin_addr.s_addr = INADDR_ANY;    // Accept connections on any of the server's addresses

    // binding
    if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        perror("bind failed!");
        exit(EXIT_FAILURE);
    }

    handleIncomingRequest(sockfd);

    close(sockfd);
    return 0;
}