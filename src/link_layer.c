// Link layer protocol implementation

#include "link_layer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include <termios.h>
#include <sys/types.h>
#include <sys/stat.h>
// MISC
#define _POSIX_SOURCE 1 // POSIX compliant source

////////////////////////////////////////////////
// LLOPEN
////////////////////////////////////////////////
#define prob_err 0.5
#define DELAY 1000
clock_t start_time;
clock_t end_time;
int BAUDRATE;
struct termios oldtio;
int tot_frames = 0;
int good_frames = 0;
int byte_received_approved = 0;
int byte_received = 0;

int alarmEnabled = FALSE;
int alarmCount = 0;
volatile int STOP = FALSE;
unsigned char frame_num_t = 0x00;
unsigned char frame_num_r = 0x40;
LinkLayer parameters;
int fd;

// Alarm function handler
void alarmHandler(int signal)
{
    alarmEnabled = FALSE;
    alarmCount++;
}

int checkSframe(char c, int count, char A, char C){
	switch(count){
		case 0:
			if(c == 0x7E)
				return 1;
			else 
				return 0;
		case 1:
			if (c == A)
				return 2;
			else if (c == 0x7E)
				return 1;
			else return 0;
		case 2:
			if (c == C)
				return 3;
			else if (c == 0x7E)
				return 1;
			else return 0;
		case 3:
			if (c == A ^ C)
				return 4;
			else if (c == 0x7E)
				return 1;
			else return 0;
		case 4:
			if (c == 0x7E)
				return 5;
			else return 0;
		default:
			return 0;				
	}
}

int sendSFrame(char A, char C){
	unsigned char buf[5];
	buf[0] = 0x7E;
	buf[1] = A;
	buf[2] = C;
	buf[3] = buf[1] ^ buf[2];
	buf[4] = buf[0];
	return write(fd, buf, 5);
}

int setconnection(char *serialPort){
	fd = open(serialPort, O_RDWR | O_NOCTTY);
	printf("%d\n", fd);
    if (fd < 0)
    {
        perror(serialPort);
        exit(-1);
    }
	/*struct termios oldtio;*/
    struct termios newtio;
	// Save current port settings
	if (tcgetattr(fd, &oldtio) == -1)
	{
		perror("tcgetattr");
		return -1;
	}
	

    // Clear struct for new port settings
    memset(&newtio, 0, sizeof(newtio));

    newtio.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;
    newtio.c_iflag = IGNPAR;
    newtio.c_oflag = 0;

    // Set input mode (non-canonical, no echo,...)
    newtio.c_lflag = 0;
    newtio.c_cc[VTIME] = 0; // Inter-character timer unused
    newtio.c_cc[VMIN] = 0;  // Read what is on the buffer right now, without waiting
	tcflush(fd, TCIOFLUSH);
	
	/*int flags = fcntl(fd, F_GETFL, 0);
	flags |= O_NONBLOCK;
	fcntl(fd, F_SETFL, flags);*/
	
	
	// Set new port settings
    if (tcsetattr(fd, TCSANOW, &newtio) == -1)
    {
        perror("tcsetattr");
        return -1;
    }

	return fd;
}

int llopen(LinkLayer connectionParameters)
{
	parameters.timeout = connectionParameters.timeout;
	parameters.nRetransmissions = connectionParameters.nRetransmissions;
	parameters.baudRate = connectionParameters.baudRate;
	BAUDRATE = connectionParameters.baudRate;
    int fd = setconnection(connectionParameters.serialPort);
	if (fd < 0) 
	{
		printf("Error in the connection\n");
		return -1;
	}
	else{
		printf("Connection ok\n");
	}
	int count = 0;
	unsigned char byte;
	switch(connectionParameters.role){
		
		case LlTx:
			(void)signal(SIGALRM, alarmHandler);
			printf("Ready to start sending\n");
			while(alarmCount < connectionParameters.nRetransmissions && count < 5){
				while(read(fd, &byte, 1) > 0 && count < 5 && alarmEnabled){
					count = checkSframe(byte, count, 0x01, 0x07);     
				}
				
				if (count == 5){
					alarm(0);
					alarmEnabled = 1;
				}
				
				if (alarmEnabled == FALSE && count < 5){
					printf("I'll try to send the frame\n");
					if (sendSFrame(0x03, 0x03) != 5){
						printf("Errore: Connection frame not sent\n");
					}
					else{
						alarm(connectionParameters.timeout);
						alarmEnabled = TRUE;
						printf("Connection frame sent\n");
					}
					
				}
				
			
			}
			usleep(DELAY); // NOT SURE YET
			while(count < 5 && read(fd, &byte, 1) > 0){ // we can put something like another timer here
					count = checkSframe(byte, count, 0x01, 0x07); 
				}
			if (count != 5)
				return -1;
			break;
			
		case LlRx:
			start_time = clock();
			usleep(DELAY); // NOT SURE YET
			while(count < 5){
				if(read(fd, &byte, 1) > 0){
					printf("%c received\n", byte);
					count = checkSframe(byte, count, 0x03, 0x03);
					byte_received++;
				}
				//sleep(1);
			}
			byte_received_approved += 5;
			
			if (sendSFrame(0x01, 0x07) != 5)
				return -1;
		
			break;
			
		default:
			return -1;
	}
	
	printf("Open phase terminated correctly\n");
    return 1;
}


int createFrame(unsigned char *buf, unsigned int bufSize, char *new_buff){
	good_frames++;
	unsigned char bcc = 0x00;
	int i; 
	int c = 4;
	// add FH 
	new_buff[0] = 0x7E;
	new_buff[1] = 0x03;
	frame_num_t = frame_num_t ^ 0x40;
	new_buff[2] = frame_num_t; 
	new_buff[3] = new_buff[1] ^ new_buff[2];
	for (i = 0; i < bufSize; i ++){ // make a function for all of the stuffing
		bcc = bcc ^ buf[i];
		if (buf[i] == 0x7E){
			new_buff[c++] = 0x7D;
			new_buff[c++] = 0x5E;
		}
		else if (buf[i] == 0x7D){
			new_buff[c++] = 0x7D;
			new_buff[c++] = 0x5D;
		}
		else
			new_buff[c++] = buf[i];
	}
	if (bcc == 0x7E){
			new_buff[c++] = 0x7D;
			new_buff[c++] = 0x5E;
		}
	else if (bcc == 0x7D){
		new_buff[c++] = 0x7D;
		new_buff[c++] = 0x5D;
	}
	else
		new_buff[c++] = bcc;
	// add FT
	new_buff[c++] = 0x7E;
	return c;
}

typedef enum{accepted, rejected}feedback;

int checkSframeR(char c, int count, feedback feed){ 
	char R;
	switch(feed){
		case accepted:
			if (frame_num_t == 0x40)
				R = 0x05;
			else
				R = 0x85;
			break;
		case rejected:
			if (frame_num_t == 0x40)
				R = 0x81;
			else
				R = 0x01;
			break;
		default:
		return 0;
	}
	switch(count){
		case 0:
			if(c == 0x7E)
				return 1;
			else 
				return 0;
		case 1:
			if (c == 0x01)
				return 2;
			else if (c == 0x7E)
				return 1;
			else return 0;
		case 2:
			if (c == R)
				return 3;
			else if (c == 0x7E)
				return 1;
			else return 0;
		case 3:
			if (c == R ^ 0x01)
				return 4;
			else if (c == 0x7E)
				return 1;
			else return 0;
		case 4:
			if (c == 0x7E)
				return 5;
			else return 0;
		default:
			return 0;				
	}
}



////////////////////////////////////////////////
// LLWRITE
////////////////////////////////////////////////

/*int alarmEnabled_w = FALSE;
int alarmCount_w = 0;
void alarmHandler_w(int signal)
{
    alarmEnabled_w = FALSE;
    alarmCount_w++;
}*/

int llwrite(unsigned char *buf, unsigned int bufSize) 
{
	printf("I'll try to write a frame\n");
	unsigned char *new_buff = malloc((2 * (bufSize + 1) + 5) * sizeof (char));
	int n_bytes = createFrame(buf, bufSize, new_buff);
	alarmCount = 0;
	alarmEnabled = FALSE;
	(void)signal(SIGALRM, alarmHandler);
	int countRR = 0, countRJ = 0;
	char byte;
	while(alarmCount < parameters.nRetransmissions && countRR < 5){
		usleep(DELAY); // NOT SURE YET
		while(read(fd, &byte, 1) > 0 && countRR < 5 && alarmEnabled){
			countRR = checkSframeR(byte, countRR, accepted);  
			if ((countRJ = checkSframeR(byte, countRJ, rejected)) == 5)
				alarmEnabled = FALSE; 	
		}
		if (countRR == 5){
			alarm(0);
			alarmEnabled = 1;
		}
		if (alarmEnabled == FALSE && countRR < 5){
			write(fd, new_buff, n_bytes);
			printf("Frame sent (%c)\n", frame_num_t);
			tot_frames ++;
			alarm(parameters.timeout);
			alarmEnabled = TRUE;
			
		}
	
	}
	while(countRR < 5 && read(fd, &byte, 1) > 0){ 
			countRR = checkSframeR(byte, countRR, accepted);
			printf("Frame sent (%c)\n", frame_num_t);
			if (countRR == 0) {
				printf("Frame sent too many times\n");
				return -1;
			}
		}
	free(new_buff); 
	if (countRR == 5){
		printf("Frame received (%c)\n", frame_num_t);
		return n_bytes;
	}
	printf("Frame sent too many times\n");
	return -1;
}

////////////////////////////////////////////////
// LLREAD
////////////////////////////////////////////////

typedef enum {current, past, disc}state;

void sendAck(char c){
	char ack;
	switch(c){
		case 0x00:
			ack = 0x85;
			break;
		case 0x40:
			ack = 0x05;
			break;
		default:
			return;
	}
	sendSFrame(0x01, ack);
}

void sendNack(){
	char nack;
	printf("I ask for the frame to be sent again (%c)\n", frame_num_r);
	switch(frame_num_r){
		case 0x00:
			nack = 0x01;
			break;
		case 0x40:
			nack = 0x81;
			break;
		default:
			return;
	}
	sendSFrame(0x01, nack);
}

int waitHeader(char c, int count, state *s){
	switch(count){
		case 0:
			if(c == 0x7E){
				if (*s != disc)
					return 1;
				else
					return -1;
			}			
			else 
				return 0;
		case 1:
			if (c == 0x03)
				return 2;
			else if (c == 0x7E)
				return 1;
			else return 0;
		case 2:
			if (c == frame_num_r)
			{
				*s = current;
				return 3;
			}	
			else if (c == 0x7E)
				return 1;
			else if (c == frame_num_r ^ 0x40){
				*s = past;
				return 3;
			}
			else if (c == 0x0B){
				*s = disc;
				return 3;
			}
			else 
				return 0;
		case 3:
			if (c == 0x03 ^ frame_num_r && (*s == current))
				return 4;
			else if (c == 0x03 ^ frame_num_r ^ 0x40 && (*s == past)){
				printf("It's not the frame I was waiting for, send ack (%c)", frame_num_r ^ 0x40);
				sendAck(frame_num_r ^ 0x40);
				return 0;
			}
			else if (c == 0x03 ^ 0x0B && (*s == disc)){
				return 0;
			}
			else if (c == 0x7E){
				return 1;
			}
			else {
				if (*s == disc)
					*s = current;
				return 0;
			}
		default:
			return 0;				
	}
}



int llread(unsigned char *packet)
{
	printf("I try to read the frame\n");
    // to wait the header
	int count = 0;
	char byte;
	state s;
	usleep(DELAY);
	while (count < 4 && count != -1){
		if (read(fd, &byte, 1) > 0){
			count = waitHeader(byte, count, &s);
			byte_received++;
		}
	}
	if (count == -1) // disc
		return 0;
	printf("Header arrived\n");
	if (count == -1) 
		return 0;
	int count_bytes = 0, end = 0, esc = 0;
	unsigned char bcc = 0x00, bcc_back; 	
	unsigned char *bits = malloc((MAX_PAYLOAD_SIZE + 1) * sizeof(unsigned char));
	while(!end && count_bytes < MAX_PAYLOAD_SIZE + 2){
		if (read(fd, &byte, 1) > 0){
			byte_received++;
			if (esc){
				switch(byte){
					case 0x5E:
						bits[count_bytes++] = 0x7E;
						bcc_back = bcc;
						bcc = bcc ^ 0x7E;
						break;
					case 0x5D:
						bits[count_bytes++] = 0x7D;
						bcc_back = bcc;
						bcc = bcc ^ 0x7D;
						break;
					default:
						printf("Not acceptable value after ESC\n");
						sendNack();
						return -1;
				}
				esc = 0;
			}
			else if (byte == 0x7D){
				esc = 1;
			}
			else if (byte == 0x7E){
				end = 1;
				count_bytes ++;
			}
			else{
				bits[count_bytes ++] = byte;
				bcc_back = bcc;
				bcc = bcc ^ byte;
			}
			
		}
	}
	srand(time(NULL)); // to put FER
	if (!end || bcc_back != bits[count_bytes - 2] || (float)rand() / RAND_MAX < prob_err){
		sendNack();
		if (!end)
			printf("Last bit wasn't the flag expected\n");
		if (bcc_back != bits[count_bytes - 2])
			printf("BCC control not passed\n");
		return -1;
	}
	sendAck(frame_num_r);
	printf("Frame received correctly, send ack (%c)\n", frame_num_r);
	frame_num_r = frame_num_r ^ 0x40;
	byte_received_approved += count_bytes + 4;
	memcpy(packet, bits, (count_bytes - 2) * sizeof(unsigned char));
	free(bits);
    return count_bytes - 2;
}

////////////////////////////////////////////////
// LLCLOSE
////////////////////////////////////////////////




int llclose(int showStatistics, LinkLayerRole role)
{	
	char byte;
	int count = 0;
	alarmEnabled = FALSE;
	alarmCount = 0;
	switch(role){
		case LlTx:
			
			(void)signal(SIGALRM, alarmHandler);
			while(alarmCount < parameters.nRetransmissions && count < 5){
				usleep(DELAY); 
				while(read(fd, &byte, 1) > 0 && count < 5 && alarmEnabled){
					count = checkSframe(byte, count, 0x01, 0x0B);     
				}
				if (count == 5){
					alarm(0);
					alarmEnabled = 1;
				}
				if (alarmEnabled == FALSE && count < 5){
					sendSFrame(0x03, 0x0B);
					alarm(parameters.timeout);
					alarmEnabled = TRUE;					
				}			
			}
			usleep(DELAY); 
			while(count < 5 && read(fd, &byte, 1) > 0){ // we can put something like another timer here
					count = checkSframe(byte, count, 0x01, 0x0B); 
						if (count == 0)
							return -1;
				}
			if (count != 5)
				return -1;
			
			sendSFrame(0x03, 0x07);
			
			if(showStatistics){
				float FER = 1.00 - (float) good_frames / (float) tot_frames;
				/*float tot_time = (float)(end_time - start_time) / (float) CLOCKS_PER_SEC;
				float R = (float) byte_received_approved * 8.0 / tot_time;*/
				printf("FER (based on real FER): %.5f\nDelay: %d us\nMaximum Size of Frame: %d\nC: %d\n", FER, DELAY, MAX_PAYLOAD_SIZE, parameters.baudRate);
			}
			break;
		
		case LlRx:
			(void)signal(SIGALRM, alarmHandler);
			
			while(alarmCount < parameters.nRetransmissions && count < 5){
				usleep(DELAY); // NOT SURE YET
				while(read(fd, &byte, 1) > 0 && count < 5 && alarmEnabled){
					count = checkSframe(byte, count, 0x03, 0x07);     
					byte_received ++;
				}
				if (count == 5){
					end_time = clock();
					alarm(0);
					alarmEnabled = 1;
					byte_received_approved += 5;
				}
				if (alarmEnabled == FALSE && count < 5){
					sendSFrame(0x01, 0x0B);
					alarm(parameters.timeout);
					alarmEnabled = TRUE;
					
				}
			
			}
			if (count != 5){
				printf("The connection has been closed because the frame DISC has been received, but the receiver didn't receive the UA frame, so something could have gone wrong, check if the file is ok\n");
				end_time = clock();
			}
			
			if(showStatistics){	
				float FER = prob_err;
				float tot_time = (float)(end_time - start_time) / (float) CLOCKS_PER_SEC;
				float R = (float) byte_received_approved * 8.0 / tot_time;
				printf("FER (just based on errors generated from me): %.5f\nDelay: %d us\nMaximum Size of Frame: %d\nC: %d\nTransference time: %.5f\n", FER, DELAY, MAX_PAYLOAD_SIZE, parameters.baudRate, R / (float) parameters.baudRate);
			}
			
			break;
		
		default:
			return -1;

	}	
	if (tcsetattr(fd, TCSANOW, &oldtio) == -1)
			{
				perror("tcsetattr");
				return -1;
			}
    return close(fd);
}
