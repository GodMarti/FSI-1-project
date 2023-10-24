// Application layer protocol implementation

#include "application_layer.h"
#include "link_layer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#define STAT 1

unsigned char *getControlPacket(char first, const char *filename, int fileSize, int *cp_size){
	unsigned char *packet = malloc(MAX_PAYLOAD_SIZE * sizeof(char));
	int size = 0, i;
	packet[size++] = first;
	packet[size++] = 0x00;
	int n_bytes = 0;
	unsigned char *size_bytes = malloc(MAX_PAYLOAD_SIZE * sizeof(char));
	while(fileSize > 0){
		size_bytes[n_bytes++] = fileSize % 256;
		fileSize = fileSize / 256;
	}
	packet[size++] = n_bytes;
	for(i = 0; i < n_bytes; i ++)
		packet[size++] = size_bytes[n_bytes - i - 1];
	free(size_bytes);
	packet[size++] = 0x01;
	packet[size++] = strlen(filename) + 1;
	for (i = 0; i < strlen(filename) + 1; i ++){
		packet[size++] = filename[i];
	}
	*cp_size = size;
	return packet;
}

unsigned char* getData(FILE *file, int fileSize){
	unsigned char* data = malloc(fileSize * sizeof(char));
	int i;
	fread(data, 1, fileSize, file); // we can add a control here
	return data;
}

unsigned char* getDataPacket(char *all_data, int data_size){
	unsigned char* packet = malloc((data_size + 3) * sizeof(char));
	packet[0] = 0x01;
	int dim, i;
	packet[1] = data_size / 256;
	packet[2] = data_size % 256;
	for(i = 0; i < data_size; i ++){
		packet[i + 3] = all_data[i];
	}
	return packet;
}

unsigned char* getName(unsigned char *packet, int *bit){
	int dim = packet[(*bit)++];
	unsigned char* file_name = malloc(dim * sizeof(char));
	for (int i = 0; i < dim; i ++)
		file_name[i] = packet[(*bit)++];
	return file_name;
}

int getSize(unsigned char *packet, int *bit){
	int dim = packet[(*bit)++];
	int size = 0;
	for (int i = 0; i < dim; i ++)
		size = size*256 + packet[(*bit)++];
	return size;
}

unsigned char* checkControlPacket(unsigned char *packet, int packetSize, unsigned long int *fileSize){
	int bit = 0,i;
	if(packet[bit++] != 0x02)
		return NULL;
	int n;
	unsigned char* file_name; 
	switch(packet[bit ++]){
		case 0x00:
			*fileSize = getSize(packet, &bit);
			if (packet[bit ++] != 0x01)
				return NULL;
			file_name = getName(packet, &bit);
			break;
		case 0x01:
			file_name = getName(packet, &bit);
			if (packet[bit ++] != 0x00)
				return NULL;
			*fileSize = getSize(packet, &bit);
			break;
		default:
			return NULL;
	}
	if (bit != packetSize) // NOT SURE ABOUT IT
		return NULL;
	return file_name;
}

int checkDataPacket(unsigned char *packet, int packetSize, unsigned char *buffer){
	if (packet[0] != 0x01 || 256*packet[1] + packet[2] != packetSize) // we have to return an error if the first three are wrong? NOT SURE ABOUT IT
		return 0;
	int i;
	for (i = 0; i < packetSize-3; i ++){
		buffer[i] = packet[i + 3];
	}
	return 1;
}

void applicationLayer(const char *serialPort, const char *role, int baudRate,
                      int nTries, int timeout, const char *filename)
{
    LinkLayer linkLayer;
    strcpy(linkLayer.serialPort,serialPort);
	if(strcmp(role, "tx") == 0)
		linkLayer.role = LlTx;
	else
		linkLayer.role = LlRx;
    linkLayer.baudRate = baudRate;
    linkLayer.nRetransmissions = nTries;
    linkLayer.timeout = timeout;

    int fd;
    if ((fd = llopen(linkLayer)) < 0) {
        perror("Connection error\n");
        exit(-1);
    }
	switch (linkLayer.role) {

        case LlTx: 
            
            FILE* file = fopen(filename, "rb");
            if (file == NULL) {
                perror("File not found\n");
                exit(-1);
            }

            int start = ftell(file);
            fseek(file,0L,SEEK_END);
            unsigned long int fileSize = ftell(file)-start;
            fseek(file,start,SEEK_SET);

            unsigned int cp_size;
            unsigned char *cpStart = getControlPacket(0x02, filename, fileSize, &cp_size);
            if(llwrite(cpStart, cp_size) == -1){ 
                printf("Exit: error in start packet\n");
                exit(-1);
            }

            /*unsigned char sequence = 0;*/
            unsigned char* all_data = getData(file, fileSize);
			unsigned char* start_data = all_data; // only for the free
            /*long int to_be_sent = fileSize;*/
			int data_size;
            while (fileSize > 0) { 
				if(fileSize > (long int) MAX_PAYLOAD_SIZE - 3)
					data_size = MAX_PAYLOAD_SIZE - 3;
				else
					data_size = fileSize;
          
                /*unsigned char* data = malloc(data_size * sizeof(char)); // I think that we can erase this
			
                memcpy(data, content, data_size);*/
                unsigned char* data_packet = getDataPacket(all_data, data_size);
                all_data += data_size;
                if(llwrite(data_packet, data_size + 3) == -1) { // maybe it's better != data_size + 3
                    printf("Exit: error in data packets\n");
                    exit(-1);
                }
                
                fileSize -= data_size; 
                free(data_packet);  
            }

            /*unsigned char *cpEnd = getControlPacket(0x03, filename, fileSize, &cpSize);*/ // do we really need to generate it again?
			cpStart[0] = 0x03;
            if(llwrite(cpStart, cp_size) == -1) { 
                printf("Exit: error in end packet\n");
                exit(-1);
            }
			free(cpStart); 
			free(start_data);
            llclose(STAT, linkLayer.role);
            break;
        

        case LlRx: 

            unsigned char *packet = malloc(MAX_PAYLOAD_SIZE * sizeof(char));
            int packetSize = -1;
            while ((packetSize = llread(packet)) < 0);
			if (packet[0] != 0x02){ // NOT SURE ABOUT IT
				printf("Exit: error in start packet\n");
				exit(-1);
			}
            unsigned long int file_size = 0;
            unsigned char* file_name = checkControlPacket(packet, packetSize, &file_size); 
			if (file_name == NULL){
				printf("Exit: error in start packet\n");
				exit(-1);
			}				
            FILE* new_file = fopen(file_name, "wb+");
			if (new_file == NULL) {
                perror("File not found\n");
                exit(-1);
            }
            while (packet[0] != 3) {    
                while ((packetSize = llread(packet)) <= 0);
                /*if(packetSize == 0) 
					stop = 1;*/
                if(packet[0] != 3){
                    unsigned char *buffer = malloc((packetSize - 3) * sizeof(char));
                    if (checkDataPacket(packet, packetSize, buffer)) 
						fwrite(buffer, sizeof(char), packetSize-3, new_file);
					else{
						printf("Exit: error in data packet\n");
						exit(-1);
					}
                    
                    free(buffer);
                }
				else{
					
					unsigned long int fileSize_check = 0;
					unsigned char* file_name_check = checkControlPacket(packet, packetSize, &fileSize_check);
					if (fileSize_check != file_size || strcmp(file_name, file_name_check) != 0){
						printf("Exit: error in end packet\n");
						exit(-1);
					}
					free(file_name_check);
					
				}
            }
			free(file_name);
			free(packet);
            fclose(new_file);
			llclose(STAT, linkLayer.role);
            break;

        default:
            exit(-1);
            break;
    }
}

