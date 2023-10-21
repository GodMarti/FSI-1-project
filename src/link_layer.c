// Link layer protocol implementation

#include "link_layer.h"

// MISC
#define _POSIX_SOURCE 1 // POSIX compliant source

////////////////////////////////////////////////
// LLOPEN
////////////////////////////////////////////////
#define BUF_SIZE 256 // NOT SURE YET
/*struct termios oldtio_t; // I want to try to declare it outside the function just because we need to restore it but the application layer cannot see it
struct termios oldtio_r;*/

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

int sendSFrame(int fd, char A, char C){
	unsigned char buf[5];
	buf[0] = 0x7E;
	buf[1] = A;
	buf[2] = C;
	buf[3] = buf[1] ^ buf[2];
	buf[4] = buf[0];
	return write(fd, buf, 5);
}

int llopen(LinkLayer connectionParameters)
{
	parameters.timeout = connectionParameters.timeout;
	parameters.nRetrasmissions = connectionParameters.nRetrasmissions;
    int fd = setconnection(connectionParameters.serialPort);
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
			while(alarmCount < connectionParameters.nRetrasmissions && count < 5){
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
					sendSFrame(fd, 0x03, 0x03);

					// Wait until all bytes have been written to the serial port
					sleep(1); // NOT SURE YET
					alarm(connectionParameters.timeout);
					alarmEnabled = TRUE;
					
				}
			
			}
			sleep(1);
			while(count < 5 && bytes = read(fd, &byte, 1) > 0){ // we can put something like another timer here
					count = checkbyte(byte, count);     
				}
			if (count != 5)
				return -1;
			break;
			
		case LlRx:
			while(count < 5){
				if(bytes = read(fd, buf, BUF_SIZE) > 0)
					count = checkSframe(buf[0], count, 0x03, 0x03);      
				//sleep(1);
			}
			
			
			if (sendSFrame(fd, 0x01, 0x07) != 5)
				return -1;
		
			break;
			
		default:
			return -1;
	}
	

    return fd;
}

int setconnection(char *serialPort, LinkLayerRole role){
	int fd = open(serialPort, O_RDWR | O_NOCTTY);
    if (fd < 0)
    {
        perror(serialPortName);
        exit(-1);
    }
	/*struct termios oldtio;*/
    struct termios newtio;
	// Save current port settings
	/*switch(role){
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
	}*/

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


int createFrame(char *buf, int bufSize, char *new_buff){
	char bcc = 0x00;
	int i;
	// to create the bcc
	for (i = 0; i < bufSize; i ++)
		ecc = bcc ^ buf[i];
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
			// Wait until all bytes have been written to the serial port
			sleep(1); // NOT SURE YET
			alarm(parameters.timeout);
			alarmEnabled = TRUE;
			
		}
	
	}
	sleep(1);
	
	while(countRR < 5 read(fd, buf, 1) > 0){ // maybe to be thrown away
			countRR = checkSframeR(buf[0], countRR, ack);
			if (count == 0) 
				return -1;
		}
	
    return n_bytes;
}

////////////////////////////////////////////////
// LLREAD
////////////////////////////////////////////////

int waitHeader(char c, int count){
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
				return 3;
			else if (c == 0x7E)
				return 1;
			else if (c == frame_num_r ^ 0x40){
				MANDA ACK
				return 0;
			}
			
			else return 0;
		case 3:
			if (c == 0x03 ^ frame_num_r)
				return 4;
			else if (c == 0x7E)
				return 1;
			else return 0;
		default:
			return 0;				
	}
}
int llread(unsigned char *packet)
{
    // to wait the header
	int count = 0;
	char byte;
	while (count < 4){
		if (read(fd, &byte, 1) > 0)
			count = waitHeader(byte, count);
	}
	int count_bytes = 0, end = 0, esc = 0;
	char bcc = 0x00; 	
	while(!end && count_bytes < MAX_PAYLOAD){
		if (read(fd, &byte, 1) > 0){
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
						ERRORE ASK FOR RETRASMISSION
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
	if (!end || bcc != packet[count_bytes - 1]){
		ERRORE ASK FOR RETRANSMISSION
		return -1;
	}
	SEND ACK

    return n_bytes;
}

////////////////////////////////////////////////
// LLCLOSE
////////////////////////////////////////////////
int llclose(int showStatistics)
{
    // TODO
	
    // Restore the old port settings
    /*if (tcsetattr(fd, TCSANOW, &oldtio) == -1)
    {
        perror("tcsetattr");
        return -1;
    }*/

    if(close(fd) < 0)
		return -1;
	return 1;
}
