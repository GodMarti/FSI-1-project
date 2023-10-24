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
// MISC
#define _POSIX_SOURCE 1 // POSIX compliant source

////////////////////////////////////////////////
// LLOPEN
////////////////////////////////////////////////
#define BUF_SIZE 256 // NOT SURE YET
#define prob_err 0.0001
#define DELAY 1
int BAUDRATE;
struct termios oldtio_t; // I want to try to declare it outside the function just because we need to restore it but the application layer cannot see it
struct termios oldtio_r;
int tot_frames = 0;
int good_frames = 0;
int byte_received_approved = 0;
int byte_received = 0;

int alarmEnabled = FALSE;
int alarmCount = 0;
volatile int STOP = FALSE;
unsigned char frame_num_t = 0x40;
unsigned char frame_num_r = 0x40;
LinkLayer parameters;
int fd;

// Alarm function handler
void alarmHandler(int signal)
{
    alarmEnabled = FALSE;
    alarmCount++;

    printf("Alarm #%d\n", alarmCount);
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

int setconnection(char *serialPort, LinkLayerRole role){
	fd = open(serialPort, O_RDWR | O_NOCTTY);
    if (fd < 0)
    {
        perror(serialPort);
        exit(-1);
    }
	/*struct termios oldtio;*/
    struct termios newtio;
	// Save current port settings
	switch(role){
		case LlTx:
			if (tcgetattr(fd, &oldtio_t) == -1)
			{
				perror("tcgetattr");
				return -1;
			}
			break;
		case LlRx:
			if (tcgetattr(fd, &oldtio_r) == -1)
			{
				perror("tcgetattr");
				return -1;
			}
			break;
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
	// Set new port settings
    if (tcsetattr(fd, TCSANOW, &newtio) == -1)
    {
        perror("tcsetattr");
        return -1;
    }

	return 1;
}

int llopen(LinkLayer connectionParameters)
{
	parameters.timeout = connectionParameters.timeout;
	parameters.nRetransmissions = connectionParameters.nRetransmissions;
	parameters.baudRate = connectionParameters.baudRate;
	BAUDRATE = connectionParameters.baudRate;
    int fd = setconnection(connectionParameters.serialPort, connectionParameters.role);
	if (fd < 0) 
	{
		printf("Error in the connection\n");
		return -1;
	}
	int count = 0;
	unsigned char byte;
	switch(connectionParameters.role){
		
		case LlTx:
			(void)signal(SIGALRM, alarmHandler);			
			while(alarmCount < connectionParameters.nRetransmissions && count < 5){
				usleep(DELAY); // NOT SURE
				while(read(fd, &byte, 1) > 0 && count < 5 && alarmEnabled){
					count = checkSframe(byte, count, 0x01, 0x07);     
				}
				if (count == 5){
					alarm(0);
					alarmEnabled = 1;
				}
				/*bytes = read(fd, buf, BUF_SIZE);
				if (bytes == 5 && buf[1] ^ buf[2] == buf[3] && buf[0] == 0x7E && buf[4] == buf[0] && buf[1] == 0x01 && buf[2] == 0x07){
					printf("Everything cool\n");
					alarm(0);
					alarmEnabled = TRUE;
					alarmCount = 3;
				}*/
				if (alarmEnabled == FALSE && count < 5){
					sendSFrame(0x03, 0x03);
					
					// Wait until all bytes have been written to the serial port
					/*sleep(1);*/ // NOT SURE YET
					alarm(connectionParameters.timeout);
					alarmEnabled = TRUE;
					
				}
			
			}
			/*sleep(1);*/
			usleep(DELAY); // NOT SURE YET
			while(count < 5 && read(fd, &byte, 1) > 0){ // we can put something like another timer here
					count = checkSframe(byte, count, 0x01, 0x07);    
				}
			if (count != 5)
				return -1;
			break;
			
		case LlRx:
			usleep(DELAY); // NOT SURE YET
			while(count < 5){
				if(read(fd, buf, BUF_SIZE) > 0){
					count = checkSframe(buf[0], count, 0x03, 0x03);
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
	

    return 1;
}


int createFrame(char *buf, int bufSize, char *new_buff){
	char bcc = 0x00;
	int i;
	// to create the bcc
	for (i = 0; i < bufSize; i ++)
		bcc = bcc ^ buf[i];
	int c = 4;
	// add FH 
	new_buff[0] = 0x7E;
	new_buff[1] = 0x03;
	frame_num_t = frame_num_t ^ 0x40;
	new_buff[2] = frame_num_t; 
	new_buff[3] = new_buff[1] ^ new_buff[2];
	for (i = 0; i < bufSize; i ++){ // make a function for all of the stuffing
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

typedef enum{ack, rej}feedback;

int checkSframeR(char c, int count, feedback feed){ 
	char R;
	switch(feed){
		case ack:
			if (frame_num_t == 0x40)
				R = 0x05;
			else
				R = 0x85;
			break;
		case rej:
			if (frame_num_t == 0x40)
				R = 0x81;
			else
				R = 0x01;
			break;
		default 
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
int llwrite(const unsigned char *buf, int bufSize) // We have to add the MAX_PAYLOAD thing
{
	char *new_buf = malloc((2 * (bufSize + 1) + 5) * sizeof (char));
	int n_bytes = createFrame(buf, bufSize, new_buff);
	alarmCount = 0;
	(void)signal(SIGALRM, alarmHandler);	
	int countRR = 0, countRJ = 0;
	while(alarmCount < parameters.nRetrasmissions && countRR < 5){
		usleep(DELAY); // NOT SURE YET
		while(read(fd, buf, 1) > 0 && countRR < 5 && alarmEnabled){
			countRR = checkSframeR(buf[0], countRR, ack);  
			if ((countRJ = checkSframeR(buf[0], countRJ, rej)) == 5)
				alarmEnabled = FALSE; // we have to check how to deal with the counter of the retransmissions		
		}
		if (countRR == 5){
					alarm(0);
					alarmEnabled = 1;
		}
		if (alarmEnabled == FALSE && countRR < 5){
			write(fd, new_buff, n_bytes);
			tot_frames ++;
			// Wait until all bytes have been written to the serial port
			sleep(1); // NOT SURE YET
			alarm(parameters.timeout);
			alarmEnabled = TRUE;
			
		}
	
	}
	//sleep(1);
	usleep(DELAY); // NOT SURE YET
	while(countRR < 5 read(fd, buf, 1) > 0){ // maybe to be thrown away
			countRR = checkSframeR(buf[0], countRR, ack);
			if (count == 0) 
				return -1;
		}
	free(new_buff);
    return n_bytes;
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
	char ack;
	switch(c){
		case 0x00:
			ack = 0x01;
			break;
		case 0x40:
			ack = 0x81;
			break;
		default:
			return;
	}
	sendSFrame(0x01, ack);
}

int waitHeader(char c, int count, state *s){
	switch(count){
		case 0:
			if(c == 0x7E)
				return 1;
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
				*s = actual;
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
			else return 0;
		case 3:
			if (c == 0x03 ^ frame_num_r && (*s == actual))
				return 4;
			else if (c == 0x03 ^ frame_num_r ^ 0x40 && (*s == past)){
				sendAck(c);
				return 0;
			}
			else if (c == 0x03 ^ 0x0B && (*s == disc)){
				return -1;
			}
			else if (c == 0x7E){
				*past = 0;
				return 1;
			}
			else {
				*past = 0;
				return 0;
			}
		default:
			return 0;				
	}
}



int llread(unsigned char *packet)
{
    // to wait the header
	int count = 0;
	char byte;
	state s;
	usleep(DELAY); // NOT SURE YET
	while (count < 4 && count != -1){
		if (read(fd, &byte, 1) > 0){
			count = waitHeader(byte, count, &s);
			byte_received++;
		}
	}
	if (count == -1) 
		return 0;
	int count_bytes = 0, end = 0, esc = 0;
	char bcc = 0x00; 	
	while(!end && count_bytes < MAX_PAYLOAD_SIZE){
		if (read(fd, &byte, 1) > 0){
			byte_received++;
			if (esc){
				switch(byte){
					case 0x5E;
						packet[count_bytes++] = 0x7E;
						bcc = bcc ^ 0x7E;
						break;
					case 0x5D:
						packet[count_bytes++] = 0x7D;
						bcc = bcc ^ 0x7D;
						break;
					default:
						sendNack();
						return -1;
				}
				esc = 0;
			}
			else if (byte == 0x7D){
				end = 1;
			}
			else{
				packet[count_bytes ++] = byte;
				bcc = bcc ^ byte;
			}
		}
	}
	srand(time(NULL));
	if (!end || bcc != packet[count_bytes - 1] || (double)rand() / RAND_MAX < prob_err){
		sendNack();
		return -1;
	}
	sendAck(frame_num_r);
	good_frames++;
	frame_num_r = frame_num_r ^ 0x40;
	byte_received_approved += count_bytes + 5;
    return count_bytes - 1;
}

////////////////////////////////////////////////
// LLCLOSE
////////////////////////////////////////////////



int alarmCount_t = 0;
int alarmEnabled_t = FALSE;

int alarmCount_r = 0;
int alarmEnabled_r = FALSE;

void alarmHandler_t(int signal)
{
    alarmEnabled_t = FALSE;
    alarmCount_t++;
}
void alarmHandler_r(int signal)
{
    alarmEnabled_r = FALSE;
    alarmCount_r++;
}


int llclose(int showStatistics, LinkLayerRole role)
{	
	char byte;
	int count;
	switch(role){
		case LlTx:
			
			(void)signal(SIGALRM, alarmHandler_t);
			while(alarmCount_t < parameters.nRetrasmissions && count < 5){
				usleep(DELAY); // NOT SURE YET
				while(read(fd, &byte, 1) > 0 && count < 5 && alarmEnabled){
					count = checkSframe(byte, count, 0x01, 0x0B);     
				}
				if (count == 5){
					alarm(0);
					alarmEnabled_t = 1;
				}
				if (alarmEnabled_t == FALSE && count < 5){
					sendSFrame(0x03, 0x0B);

					// Wait until all bytes have been written to the serial port
					sleep(1); // NOT SURE YET
					alarm(parameters.timeout);
					alarmEnabled_t = TRUE;					
				}			
			}
			//sleep(1);
			usleep(DELAY); // NOT SURE YET
			while(count < 5 && read(fd, &byte, 1) > 0){ // we can put something like another timer here
					count = checkSframe(byte, count, 0x01, 0x0B);     
				}
			if (count != 5)
				return -1;
			
			sendSFrame(0x03, 0x07);
			
			if (tcsetattr(fd, TCSANOW, &oldtio_t) == -1)
			{
				perror("tcsetattr");
				return -1;
			}
			break;
		
		case LlRx:
			(void)signal(SIGALRM, alarmHandler_r);
			
			while(alarmCount_r < parameters.nRetrasmissions && count < 5){
				usleep(DELAY); // NOT SURE YET
				while(read(fd, &byte, 1) > 0 && count < 5 && alarmEnabled_r){
					count = checkSframe(byte, count, 0x03, 0x07);     
					byte_received ++;
				}
				if (count == 5){
					alarm(0);
					alarmEnabled_r = 1;
				}
				if (alarmEnabled_r == FALSE && count < 5){
					sendSFrame(0x01, 0x0B);

					// Wait until all bytes have been written to the serial port
					sleep(1); // NOT SURE YET
					alarm(parameters.timeout);
					alarmEnabled_r = TRUE;
					
				}
			
			}
			byte_received_approved += 5;
			/*sleep(1);
			while(count < 5 && bytes = read(fd, &byte, 1) > 0){ // we can put something like another timer here
					count = checkSframe(byte, count, 0x03, 0x07);      
				}
			if (count != 5)
				return -1;*/
			
			if (tcsetattr(fd, TCSANOW, &oldtio_r) == -1)
			{
				perror("tcsetattr");
				return -1;
			}
			break;
		
		default:
			return -1;

	}
	if(showStatistics){
		float FER = 1.00 - (float) good_frames / (float) tot_frames;
		printf("FER: %.2f\nDelay: %d us\nMaximum Size of Frame: %d\nC: %d\nTransference time: %.5f", FER, DELAY, MAX_PAYLOAD_SIZE, parameters.baudRate, (float) byte_received_approved * 8.0 / (float) parameters.baudRate);
	}
    return close(fd);
}
