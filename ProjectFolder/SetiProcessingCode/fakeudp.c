/* fpont 12/99 */
/* pont.net    */
/* udpClient.c */

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h> /* memset() */
#include <sys/time.h> /* select() */ 



#define REMOTE_SERVER_PORT 2010
#define MAX_MSG 100

#define BYTE_SWAPPING 1

int endianSwap32(int x);

typedef struct power_data_s {
	int PFBbinNumber;
	int meanPower;
	int error;
	int binNumber;
	int peak;
} power_data;

power_data getData()
{
	static int PFBbinNumber = 4095;
	static int meanPower = 1000;
	static int peaksLeftPerBin = 0;
	power_data data;

	if(peaksLeftPerBin == 0){
		//increment PFBbinNumber
		PFBbinNumber++;
		PFBbinNumber%=4096;

		meanPower += rand()%15-7;
		if(meanPower < 200){
			meanPower = 220;
		}
		if(meanPower > 999){
			meanPower = 979;
		}
		data.meanPower = meanPower;

		//pick number of peaks for next bin
		peaksLeftPerBin = rand()%2000;
		if(peaksLeftPerBin < 700){
			peaksLeftPerBin = 0;
		}
		else if(peaksLeftPerBin < 1200){
			peaksLeftPerBin = 0;
		}
		else if(peaksLeftPerBin < 1600){
			peaksLeftPerBin = 0;
		}
		else if(peaksLeftPerBin < 1800){
			peaksLeftPerBin = 1;
		}
		else if(peaksLeftPerBin < 1970){
			peaksLeftPerBin = 2;
		}
		else{
			peaksLeftPerBin = rand()%23 + 5;
		}
		// MAKES EVERY PACKET SAME SIZE
		peaksLeftPerBin = 16;

		data.binNumber = 0;
		data.peak = 0;
		data.PFBbinNumber = PFBbinNumber;

	}
	else{
		data.PFBbinNumber = PFBbinNumber;
		peaksLeftPerBin--;
		data.binNumber = rand()%32768;
		data.meanPower = meanPower;
		// pick random peak data
		data.peak = rand()%100;
		if(data.peak < 40){
			data.peak = rand()%200 + data.meanPower;
		}
		else if(data.peak < 60){
			data.peak = rand()%350 + data.meanPower;
		}
		else if(data.peak < 90){
			data.peak = rand()%500 + data.meanPower;
		}
		else if(data.peak < 100){
			data.peak = rand()%1000 + data.meanPower;
		}
		data.binNumber = 2048*peaksLeftPerBin + 1024;
		data.peak = data.meanPower + 1;
//		printf("meanPower is %i, peak is %i\n", data.meanPower, data.peak);
	}

	// binary point stuff;
	data.peak *= 2147483/2000;
	data.meanPower *= 2147483/2000;	
	
//	printf("meanPower is %i, peak is %i\n......\n", data.meanPower, data.peak);
	
	data.error = 0;
	return data;
}



int main(int argc, char *argv[]) {

	int ggg;
	int sd, rc, i, j;
	struct sockaddr_in cliAddr, remoteServAddr;
	struct hostent *h;
	power_data data;
	power_data lastData;
	int array[259];
	int size;
	int PFBnum;
	int counter;
	FILE *fpToWrite;

	// check command line  args
	if(argc<2) {
		printf("usage : %s <server> <data1> ... <dataN> \n", argv[0]);
		exit(1);
	}
	if(argc == 3){
		if(strcmp(argv[2], "-p") == 0){
			fpToWrite = fopen("/tmp/fakeudpPID","w+t");
			fprintf(fpToWrite, "%i", getpid());
			fclose(fpToWrite);
		}
	}

	// get server IP address (no check if input is IP address or DNS name
	h = gethostbyname(argv[1]);
	if(h==NULL){
		printf("%s: unknown host '%s' \n", argv[0], argv[1]);
		exit(1);
	}

	printf("%s: sending data to '%s' (IP : %s) \n", argv[0], h->h_name, inet_ntoa(*(struct in_addr *)h->h_addr_list[0]));

	remoteServAddr.sin_family = h->h_addrtype;
	memcpy((char *) &remoteServAddr.sin_addr.s_addr, h->h_addr_list[0], h->h_length);
	remoteServAddr.sin_port = htons(REMOTE_SERVER_PORT);

	// socket creation
	sd = socket(AF_INET,SOCK_DGRAM,0);
	if(sd<0){
		printf("%s: cannot open socket \n",argv[0]);
		exit(1);
	}


	// bind any port
	cliAddr.sin_family = AF_INET;
	cliAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	cliAddr.sin_port = htons(0);

	rc = bind(sd, (struct sockaddr *) &cliAddr, sizeof(cliAddr));
	if(rc<0){
		printf("%s: cannot bind port\n", argv[0]);
		exit(1);
	}


	// send data
	lastData = getData();
	counter=0;

	while(1){
		if(BYTE_SWAPPING){
			array[0] = endianSwap32(data.PFBbinNumber);
			array[1] = endianSwap32(data.meanPower);
			array[2] = endianSwap32(data.error);
		}
		else{
			array[0] = data.PFBbinNumber;
			array[1] = data.meanPower;
			array[2] = data.error;
		}
		
		size = 12;
		i=3;
		data = getData();
		while(data.PFBbinNumber == PFBnum){
			if(BYTE_SWAPPING){
				array[i] = endianSwap32(data.binNumber);
				array[i+1] = endianSwap32(data.peak);
			}
			else{
				array[i] = data.binNumber;
				array[i+1] = data.peak;
			}
			size += 8;
			data = getData();
			i+=2;
		}
		lastData = data;
		PFBnum = lastData.PFBbinNumber;

		//send the array
		rc = sendto(sd, array, size, 0, (struct sockaddr *) &remoteServAddr, sizeof(remoteServAddr));
		if(rc > 0){
			counter++;
			if(counter%10000 == 0){
				printf("Sent packet number %i\n", counter);
			}
			for (ggg = 0; ggg <= 30000;ggg++);
		}
/*		if(counter >= 500000){
			printf("Last packet number %i\n", counter);
			exit(0);
		}
*/
		if(rc<0){
		printf("%s: cannot send data \n",argv[0]);
		close(sd);
		exit(1);
		}
	//	for(j=0; j<10000000; j++);
	}
	return 0;
}


int endianSwap32(int x)
{
	char swapped[4];
	char *pointer = (char *)&x;
	swapped[0] = pointer[3];
	swapped[1] = pointer[2];
	swapped[2] = pointer[1];
	swapped[3] = pointer[0];
	return *(int *)swapped;
}
