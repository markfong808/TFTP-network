//for system calls, please refer to the MAN pages help in Linux
//TFTP server program over udp - CSS432 
#include <cstdio>
#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <algorithm>
#include <filesystem>
#include <sys/select.h>
#include <cerrno>
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
    bool transmission_complete = false;

    for (;;) {
         //timer
         fd_set readfds; //represent a set of file descriptors
         FD_ZERO(&readfds);
         FD_SET(sockfd, &readfds);
         
         //timeout struct
         struct timeval timeout;
         timeout.tv_sec = 1;
         timeout.tv_usec = 0;
         
         
         //timer start before receiving next upcoming data packet
         int timer = select(sockfd + 1, &readfds,NULL,NULL,&timeout);
         
         if(timer < 0){
             std::cout << "Error occure in select " << std::endl;
             break;
         } if(timer == 0){
            
            //Timeout occurred
            handleTimeout(retryCount);
            if (retryCount >= MAX_RETRY_COUNT) {
                std::cout << "Maximum retries reached limit. Aborting transmission." << std::endl;               
               //break;
            }
         } else {
            //data ready to transmit
         
         
        /*
         * TODO: Receive the 1st request packet from the client
         */
        ssize_t num_bytes = recvfrom(sockfd, buffer, sizeof(buffer), 0, (struct sockaddr *)&cli_addr, &addrlen);
        // printBuffer(buffer, sizeof(buffer));
        
        /*
         * TODO: Parse the request packet. Based on whether it is RRQ/WRQ, open file for read/write.
         * Create the 1st response packet, send it to the client.
         */
        uint16_t opcode = parseOpcode(buffer);
        //std::cout << "Opcode: " << opcode << std::endl;

        // Check if it's an RRQ or WRQ
        if (opcode == TFTP_RRQ)
        {
            // Extract filename and mode from the packet
            char filename[255];
            char mode[10];
            strncpy(filename, buffer + 2, 255);
            strncpy(mode, buffer + 2 + strlen(filename) + 1, 10);

            // Print the extracted filename and mode
            std::cout << "Requested filename is: " << filename << std::endl;
            //std::cout << "Mode: " << mode << std::endl;
                        
            // creating full path
            std::string filePath = std::string(SERVER_FOLDER) + std::string(filename);
            //std::cout << "Full path: " << filePath << std::endl;            
            std::ifstream file(filePath, std::ios::binary);
            char ErrorMsgbuffer[MAX_PACKET_LEN];
            unsigned short errorcode = 1; //file does not exist error code
            bool error = false;
            if (access(filePath.c_str(), F_OK) == -1) //does not exist
            {
                // File does not exist
                std::cout << "File does not exist!" << std::endl;
                const char *errorMessage = "File does not exist: ";
                error = true;
                size_t errorMsgLength = strlen(errorMessage);
                const size_t filePathLength = filePath.size();
                const size_t totalLength = errorMsgLength + filePathLength + 1;
                strncpy(ErrorMsgbuffer + 4, errorMessage, errorMsgLength);
                strncpy(ErrorMsgbuffer + 4 + errorMsgLength + 1, filePath.c_str(), filePathLength);
                ErrorMsgbuffer[4 + errorMsgLength + filePathLength + 1] = '\0'; // Null-terminate the string
                createErrorPacket(ErrorMsgbuffer, errorcode, totalLength);
                size_t BytesSend = sendto(sockfd, ErrorMsgbuffer, 4 + totalLength + 1, 0, (struct sockaddr *)&cli_addr, sizeof(cli_addr));
                
            } else { //proceed noraml transmittion

            int packet = 1;
            unsigned short blockNumber = 1;
            char Data_buffer[MAX_PACKET_LEN];
            while (true)
            {
                file.read(Data_buffer + 4, MAX_PACKET_LEN - 4);
                size_t bytesRead = file.gcount();
                char *pos = Data_buffer;
                pos += 4;
                std::cout << "Requested File chunk size " << bytesRead << std::endl; 
                std::cout << "Creating Packet No. " << packet++ << std::endl;
                createDataPacket(Data_buffer, blockNumber, bytesRead, pos);
                //printBuffer(Data_buffer, MAX_PACKET_LEN);
                size_t BytesSend = sendto(sockfd, Data_buffer, 4 + bytesRead, 0, (struct sockaddr *)&cli_addr, sizeof(cli_addr));
                std::this_thread::sleep_for(std::chrono::milliseconds(200)); //delay sending
                pos += bytesRead;
                blockNumber++;

                if (file.eof())
                {
                    transmission_complete = true;
                    break;
                }
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
            //std::cout << "Full path: " << filePath << std::endl;
            
            char ErrorMsgbuffer[MAX_PACKET_LEN];
            unsigned short errorcode = 6;
            bool error = false;
            if(access(filePath.c_str(),F_OK) != -1){
                // File already exists
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
            int block_n = 1;

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
                        
                        if(block == block_n){
                            std::string filePath = std::string(SERVER_FOLDER) + std::string(filename);
                            writeFile(filePath, receiver, BytesReceived);
                            block_n++;
                            // send ACK check for received block
                            char Ackbuffer[TFTP_ACK];
                            createAckPacket(opcode, block, Ackbuffer);
                            // printBuffer(Ackbuffer,TFTP_ACK);
                            sendto(sockfd, Ackbuffer, sizeof(Ackbuffer), 0, (struct sockaddr *)&cli_addr, sizeof(cli_addr));
                        } else {
                            // send ACK check for received block
                            char Ackbuffer[TFTP_ACK];
                            createAckPacket(opcode, block, Ackbuffer);
                            // printBuffer(Ackbuffer,TFTP_ACK);
                            sendto(sockfd, Ackbuffer, sizeof(Ackbuffer), 0, (struct sockaddr *)&cli_addr, sizeof(cli_addr));
                            continue;
                        }

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
            } 
          } 
        }
        else if (opcode == TFTP_ACK)
        {
            int block = parseBlockNumber(buffer);

            std::cout
                << "Received Ack "
                << "#" << block << std::endl;
        }
        else //illegal opcode handler
        {
            char filename[255];
            char mode[10];
            strncpy(filename, buffer + 2, 255);
            strncpy(mode, buffer + 2 + strlen(filename) + 1, 10);
            std::cout << "Requested filename is: " << filename << std::endl;
            char ErrorMsgbuffer[MAX_PACKET_LEN];
            unsigned short errorcode = 4;
    
            std::cout << "Received illegal opcode: " << opcode << std::endl;
            const char *errorMessage = "Illegal Opcode!";
            size_t errorMsgLength = strlen(errorMessage);
            strncpy(ErrorMsgbuffer + 4, errorMessage, errorMsgLength);
            ErrorMsgbuffer[4 + errorMsgLength + 1] = '\0'; // Null-terminate the string
            createErrorPacket(ErrorMsgbuffer, errorcode, errorMsgLength);
            size_t BytesSend = sendto(sockfd, ErrorMsgbuffer, 4 + errorMsgLength + 1, 0, (struct sockaddr *)&cli_addr, sizeof(cli_addr));
            
        }
        
     retryCount = 0; //reset the retry count
     
     } //else statment for timer > 0 

  } 
} // handle incoming request function end

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