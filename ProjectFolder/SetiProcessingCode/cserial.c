/* bee2 & ibob realtime serial plotter              */
/* Daniel Chapman, Andrew Siemion, Pierre Yves-Droz */
/* SSL, BWRC Collabo  -- July 2005                  */
/* dependencies: gnuplot 4.0 (www.gnuplot.org)      */
/*               FFTW        (www.fftw.org)         */




#include <stdio.h>   /* Standard input/output definitions */
#include <string.h>  /* String function definitions */
#include <unistd.h>  /* UNIX standard function definitions */
#include <fcntl.h>   /* File control definitions */
#include <errno.h>   /* Error number definitions */
#include <termios.h> /* POSIX terminal control definitions */
#include "/home/danw/SPECTROSUITE/gnuplot_i-2.10/src/gnuplot_i.h"
#include <memory.h>  /* Mallocation Station */
#include <fftw3.h>   /* Fast Fourier Transform functions */
#include <math.h>    /* One plus one is two */
#include <signal.h>  /*Signal functions*/


/*   Globals    */
#define BUFFER_SIZE 1048576
#define COMMAND_LENGTH 10

int fd;              /* File descriptor for the port */
int fftFlag = 0;
fftw_plan p;
      fftw_complex *in, *out;



void quit();
void toggleFFT();
char *itoa(int value);

int main(void)
{
      
   
      gnuplot_ctrl *h1;    /* structure required by gnuplot pseudo-api */

     
      double x[65536];
      double y[65536];      

       int result;
      

//      struct termios options;
      char buffer[BUFFER_SIZE];
      char COMMAND[COMMAND_LENGTH];
      int i;
      int rows, columns;
      int oldRows = 0;
      double matrix[65536][2];
      int tmpbuf, tmpbuf2;
      char tmpbuffer[4];
      int skip;
      int processID;
      char fftCommand[30];

      processID = getpid();

      strcpy(fftCommand, "bind f \"!kill -14 ");
      strcat(fftCommand, itoa(processID));
      strcat(fftCommand, "\"");


       strcpy(COMMAND, "adcsample\n");

       signal(SIGHUP, quit);
       signal(SIGINT, quit);
       signal(SIGQUIT, quit);
       signal(SIGTERM, quit);

       signal(14, toggleFFT);



       h1 = gnuplot_init();
       gnuplot_cmd(h1, "set mouse");
       gnuplot_cmd(h1, fftCommand);


	  p = fftw_plan_dft_1d(1, matrix, matrix, FFTW_FORWARD, FFTW_ESTIMATE);
      
      fd = open("/dev/ttyS0", O_RDWR | O_NOCTTY | O_NDELAY | O_NONBLOCK);
      if (fd == -1){
	perror("open_port: Unable to open port.");
      }
      else{
	//	fcntl(fd, F_SETFL, 0);
	printf("Serial port up...\n");
      }

      write(fd, "\n", 1);
      printf("Searching for percent ... \n");
      do{
	result = read(fd, buffer, 1);
      }while(buffer[0] != '%');
      


    do{
      skip = 0;
      memset(tmpbuffer, 0x0, 4 * sizeof(char));
      memset(buffer, 0x0, BUFFER_SIZE * sizeof(char));
      memset(&x, 0, 65536 * sizeof(double));
      memset(&y, 0, 65536 * sizeof(double));
      memset(&matrix, 0, 65536 * 2 * sizeof(double));
      memset(&tmpbuf, 0, sizeof(int));
      memset(&tmpbuf2, 0, sizeof(int));

      do{
       
	result = write(fd, COMMAND, COMMAND_LENGTH);
      }while(result != COMMAND_LENGTH);

      i=0;
      do{
	result = read(fd, buffer+i, 1);
	if(result == 1){
	  i++;
	}
	//printf("i=%i\n", i);
      }while(buffer[i-1] != '%');

      sscanf(buffer+COMMAND_LENGTH+2, "%08X\t%08X\t", &rows, &columns);
      printf("rows: %i, cols: %i\n", rows, columns);      
      

        if ((rows == 0) || (columns == 0)) {
	printf("It appears the serial device isn't ready yet, attempting to clear serial buffer...\n");
      
        
	do {
	  
          result = read(fd, tmpbuffer, 4);   
	  }  while( strcmp(tmpbuffer, "IBOB") != 0);

        printf("Trying again...\n");
        skip = 1;
            
          }

	      if (skip != 1) {

      //Account for transposed matrix - deprecated

      if (columns > rows) {

	if (rows == 1){
	  for (i = 0; i < columns; i++){
       
            sscanf(buffer+COMMAND_LENGTH+22+(i*9), "%08X\t", &tmpbuf);
	    matrix[i][0] = (double) tmpbuf;
             }
	 } else {
	   for ( i = 0; i < columns; i++){
	     sscanf(buffer+COMMAND_LENGTH+22+(i*9), "%08X\t%08X\t", &tmpbuf, &tmpbuf2);
             matrix[i][0] = (double) tmpbuf;
	     matrix[i][1] = (double) tmpbuf2;
           }


	}

	//complete transpose
        columns = i;
        columns = rows;
	rows = i;


      }else{




	//if(columns > 2){
	//printf("Uhh.. stop sending more than 2 columns.\nThere should only be reals and imaginaries.\n");
	//columns = 2;
	//}


      for(i=0; i<rows; i++){
	sscanf(buffer+COMMAND_LENGTH+22+(i*9), "%08X\t", &tmpbuf);
        matrix[i][0] = (double) tmpbuf;
      }
      if(columns > 1){
	for(i=0; i<rows; i++){
	  sscanf(buffer+COMMAND_LENGTH+22+rows*9+i*9, "%08X\t", &tmpbuf);
	  matrix[i][1] = (double) tmpbuf;
         }
      }

      }

      /*
      for(i=0; i<rows; i++){
	for(j=0; j<columns; j++){
	  printf("%f, ", matrix[i][j]);
	}
	printf("\n");
      }
      */

 

 
   
  /* Configure FFT Plan -- see www.fftw.org */

      if(fftFlag){
	if(rows != oldRows){
	  fftw_destroy_plan(p);	  
	  p = fftw_plan_dft_1d(rows, matrix, matrix, FFTW_FORWARD, FFTW_ESTIMATE);
	}
	oldRows = rows;
	fftw_execute(p);
      }


     for(i=0; i < rows; i++){
	x[i] = (double) i;
        
        y[i] = sqrt( (matrix[i][0] * matrix[i][0])  + (matrix[i][1] * matrix[i][1]) ) ;         
	  }



      gnuplot_resetplot(h1);
      gnuplot_setstyle(h1, "lines");
      gnuplot_plot_xy(h1, x, y, rows, "stuff");
      
      }    



    }while(1);

    

   
    quit();
}

void quit(){

  printf("Exiting....\n");
  fftw_destroy_plan(p);
   fftw_free(in); fftw_free(out);

   //fclose(fd);
   exit(0);
}

void toggleFFT()
{
  fftFlag = (fftFlag+1)%2;
}

char *itoa(int value)
{
int count,                   /* number of characters in string       */
    i,                       /* loop control variable                */
    sign;                    /* determine if the value is negative   */
char *ptr,                   /* temporary pointer, index into string */
     *string,                /* return value                         */
     *temp;                  /* temporary string array               */

count = 0;
if ((sign = value) < 0)      /* assign value to sign, if negative    */
   {                         /* keep track and invert value          */
   value = -value;
   count++;                  /* increment count                      */
   }

/* allocate INTSIZE plus 2 bytes (sign and NULL)                     */
temp = (char *) malloc(sizeof(int) + 2);
if (temp == NULL)
   {
   return(NULL);
   }
memset(temp,'\0', sizeof(int) + 2);

string = (char *) malloc(sizeof(int) + 2);
if (string == NULL)
   {
   return(NULL);
   }
memset(string,'\0', sizeof(int) + 2);
ptr = string;                /* set temporary ptr to string          */

/*--------------------------------------------------------------------+
| NOTE: This process reverses the order of an integer, ie:            |
|       value = -1234 equates to: char [4321-]                        |
|       Reorder the values using for {} loop below                    |
+--------------------------------------------------------------------*/
do {
   *temp++ = value % 10 + '0';   /* obtain modulus and or with '0'   */
   count++;                      /* increment count, track iterations*/
   }  while (( value /= 10) >0);

if (sign < 0)                /* add '-' when sign is negative        */
   *temp++ = '-';

*temp-- = '\0';              /* ensure null terminated and point     */
                             /* to last char in array                */

/*--------------------------------------------------------------------+
| reorder the resulting char *string:                                 |
| temp - points to the last char in the temporary array               |
| ptr  - points to the first element in the string array              |
+--------------------------------------------------------------------*/
for (i = 0; i < count; i++, temp--, ptr++)
   {
   memcpy(ptr,temp,sizeof(char));
   }

return(string);
}
