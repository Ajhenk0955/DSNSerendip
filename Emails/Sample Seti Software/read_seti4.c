// read_seti4.c
/* April 07, 2010 - Ray Bambery                             */
/*  This program was derived from read_seti.c written       */
/*  by Tom Kuiper                                           */
/*  It runs on a machine called bee2spec and reads data     */
/*  from a socket to a machine called beegentle which       */
/*  is interfaced to a spectrometer.                        */
/*  The data format as described below. This program        */
/*  reformats the data to separate out a spectrum.          */
/*  The spectrum number is assigned in this program         */
/*  The spectrum is subdivided into 2 subelements:          */
/*  The pfb_bin and the fft_bin whose numbers are assigned  */
/*  on beegentle.                                           */
/*  There are 4096 coarse bins. The                         */

/* Apr 25, 2010 - changed output of .hit to final form      */
/* May 19, 2010 - added get_limit routine to parse the      */
/*                  bee_config_buffer for max_hits          */
/*                  Fixed error in logic of determining     */
/*                  max_hits                                */
/* May 20, 2010 - removed max_hits from parameters. It      */ 
/*                  was there only for earlier versions     */
/*                  didnt parse the config buffer           */
#include <math.h>
#include <stdio.h>
#include <grace_np.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include "setispec.h"
#include "casper.h"
#include "gracie.h"

#define BEE_CONFIG_RECV_PORT 31337
#define BEE_CONFIG_PACKET_SIZE 512
/* prototypes */
void get_args(int argc, char** argv,int* num_files,int* num_spectra );
int get_limit (char *buffer);
int get_bee_config();

char bee_config_buffer[BEE_CONFIG_PACKET_SIZE];
int run_get_bee_config=1;
/********************************************************************/
/* get_args                                                         */
/********************************************************************/

void get_args(int argc, char** argv,
              int* num_files,
              int* num_spectra ) {
    int i;
    /* default values */
    /* Start at i = 1 to skip the command name. */
    for (i = 1; i < argc; i++) {
      /* Check for a switch (leading "-"). */
      if (argv[i][0] == '-') {
      /* Use the next character to decide what to do. */
        switch (argv[i][1]) {
          case 'n': *num_files = atoi(argv[++i]);
            break;
          case 's': *num_spectra = atoi(argv[++i]);
            break;
          default:  fprintf(stderr,
            "Usage: read_seti4 {-s} {-n}\n"
            " where s = number of spectra to read per output file"
                        " (default 540)\n"
            "       n = number of output files (default 1)\n"
            "            (if 0, then 100000)\n");
          exit(-1);
        }
      }
    }
}
/********************************************************************/
/********************************************************************/
/* get_bee_config                                                   */
/* get values from bee2.config file                                 */
/*      max_hits_per_bin                                            */
/*      scale_threshold                                             */
/*                                                                  */
/*  The shell script sendstatus.sh on beegentle sends the info      */
/*  to fill in the configuration buffer.                            */
/*  Sendstatus will be issuing Updating...  messages as it is       */
/*  running on beegentle.   If sendstatus is not running the the    */
/*  socket bind call will just hang forever.  Thus, the warning     */
/*  message below #######                                           */
/*                                                                  */
int get_bee_config()
{
    int i;
    int numbytes;
    int limit;
    char *beebuff;

/* Initialize bee_config_buffer */
    for(i=0;i<512;i++)
    {
        bee_config_buffer[i] = '\0';
    }

    int sockfd;
    struct sockaddr_in recv_addr;
    struct sockaddr_in send_addr;
    socklen_t addr_len;

    if((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
    {
        perror("??E - beeconfig: open socket error");
        exit(1);
    }
    recv_addr.sin_family = AF_INET;
    recv_addr.sin_port = htons(BEE_CONFIG_RECV_PORT);
    recv_addr.sin_addr.s_addr = INADDR_ANY;

printf ("################################################################################\n");
printf ("# If the BEE config buffer: numbytes = 152 message doesnt  show up for >15 sec #\n");
printf ("#   then sendstatus and the 4 bofs are not running on beegentle                #\n");
printf ("################################################################################\n\n");
fflush(stdout);
    memset(recv_addr.sin_zero, '\0', sizeof recv_addr.sin_zero);
    if(bind(sockfd, (struct sockaddr *)&recv_addr, sizeof recv_addr) == -1)
    {
        perror("??E - beeconfig: bind socket error");
        exit(1);
    }
    addr_len = sizeof send_addr;
        if((numbytes =
             (int)recvfrom(sockfd,bee_config_buffer,BEE_CONFIG_PACKET_SIZE, 1,
             (struct sockaddr *)&send_addr, &addr_len)) == -1)
        {
            perror("??E - beeconfig: recvfrom socket error");
            exit(1);
        }
        printf("***** BEE config buffer:   numbytes = %d *****\n",numbytes);
        if (numbytes < 150) {
            if((numbytes =
                 (int)recvfrom(sockfd,bee_config_buffer,BEE_CONFIG_PACKET_SIZE, 1,
                (struct sockaddr *)&send_addr, &addr_len)) == -1)
            {
                perror("??E - beeconfig: recvfrom socket error");
                exit(1);
            }
            printf("***** BEE config buffer:   numbytes = %d *****\n",numbytes);
        }


//        printf("%s \n",bee_config_buffer);
    close(sockfd);

    beebuff = &bee_config_buffer[0];
    limit = get_limit(beebuff);
    return  (limit);
}
/*************************************************************************/
int get_limit (char *buffer) {

    const char token_sep [] = ":\n";        /* separators */
    char *pt;
    const char thlim [] = "THRESH LIMIT";
    int flag=0, limit=0;
//    printf ("get_limit: buffer = %s",buffer);
  
    pt = strtok (buffer, token_sep);
    while (pt) {
        pt = strtok(NULL, token_sep); 
//        printf ("token = %s\n",pt);
        if (flag == 1) {
            limit = (int) atoi(pt);
//            printf (">limit = %d\n",limit);
            flag = 0;
            break;
        }
        if (strcmp(pt,thlim) == 0) {flag=1;}

     }
//    printf ("limit = %d\n",limit);
    return limit; 
}
/*************************************************************************/
/*************************************************************************/
/*************************************************************************/
int main(int argc, char** argv) {
    int tmp;

    /* Ray Bambery - Oct 02, 2009                                                           */
    /* before this program will work the following processes must be active                 */
    /* on beegentle. Invoking ps -ef on beegentle should show:                              */
    /*   /home/obs/bofs/pfb_phaseshift.bof                                                  */
    /*   /home/obs/bofs/ct_phaseshift.bof                                                   */
    /*   /home/obs/bofs/fft_phaseshift.bof                                                  */
    /*   /home/obs/bofs/thr_phaseshift.bof                                                  */
    /*   /home/obs/bin/sendstatus.sh                                                        */
    /* otherwise the get_bee_config routine will hang                                       */
    /*                                                                                      */
    /* if sendstatus.sh is running but the 4 bofs are not then                              */
    /*      get_bee_config will return:                                                     */
    /*                                                                                      */
    /*  BEE config buffer:   numbytes = 500                                                 */
    /*  BEE2 STATUS: Thu Oct  1 01:29:26 PDT 2009                                           */
    /*  PFB SHIFT: Error: couldn't open file '/proc/22177/hw/ioreg/fft_shift'               */
    /*  FFT SHIFT: Error: couldn't open file '/proc/22185/hw/ioreg/fft_shift'               */
    /*  THRESH LIMIT: Error: couldn't open file '/proc/22183/hw/ioreg/thr_comp1_thr_lim'    */
    /*  THRESH SCALE: Error: couldn't open file '/proc/22183/hw/ioreg/thr_scale_p1_scale'   */
    /*  TENGE PORT: Error: couldn't open file '/proc/22183/hw/ioreg/rec_reg_10GbE_destport0'*/
    /*  TENGE IP: Error: couldn't open file '/proc/22183/hw/ioreg/rec_reg_ip'               */
    /*                                                                                      */
    /*  and the program will run but give bad data                                          */
    /*                                                                                      */
    /*  you will also get the following messages:                                           */
    /*      Lost data collecting spectrum 0 at channel 480 and PFB bin 1819                 */
    /*      Lost data collecting spectrum 0 at channel 481 and PFB bin 1820                 */
    /*      ...                                                                             */
    /*      ...                                                                             */
    /*  which I have truncated internally to stop repeating after 100 such messages         */
    /*                                                                                      */
   /* All signed numbers are represented as two's complement signed. If a data
        bus is given as type X.Y, that means it is an X-bit number with a binary
        point at the Yth bit. Examples:
        int:   X.0
        float: If a number is an 8.7 signed number that means it is 8 bits, with
           the binary point at 7, so it is in the range [-1, 1-(2^-7)], or
           [-1, 0.9921875]. 
    */
    int fft_bin       = 0;      // 15.0
    int pfb_bin       = 0;      // 12.0
    int over_thresh   = 0;      //  1.0
    int blank         = 0;      //  3.0
    int event         = 0;      //  1.0
    unsigned long pfb_fft_power = 0;      // 32.0
    double maxvalue    = 0;
    // loop counters
    int j             = 0;
    int k             = 0;
    int l             = 0;
    // variables associated with socket
    int                sockfd;
    struct sockaddr_in recv_addr;       // recv address information
    struct sockaddr_in send_addr; // connector's address information
    socklen_t          addr_len;
    int        numbytes;
    FILE       *hdr_file;
    FILE       *data_file;
    FILE       *hit_file;
    const char hdr_file_str[200];       //spc00000.hdr
    const char data_file_str[200];      //spc00000.crs
    const char hit_file_str[200];       //spc00000.hit
    /* COARSEBINS is the number of PFB channels, defined as 4096 in
    setispec.h  */
    int        spectra[COARSEBINS];
    int        channel_ctr = -1;
    unsigned int        spectrum_ctr = 0;
    int        hit_ctr = 0;
    int        synced = 0;
    int         data_ptr_cnt = 0;
    int         file_full = 0;

    int         maxpower = 0;
    int         maxpower_hit_ctr = 0;
    int         maxpower_spectrum = 0;
    int         maxpower_pfb_bin = 0;
    int         maxpower_fft_bin = 0;

//    unsigned int log1000power;
    uint16_t  log100power;
    float   db_power;
//    uint16_t  t66 = 66;

//uint16_t - old debug format
//    struct hit_struct{
//      uint16_t key;                 // unsigned int
//      uint16_t pfb_bin;             // unsigned int
//      uint16_t fft_bin;             // unsigned int
//      uint16_t sixtysix;
//      unsigned int hitctr;
//      unsigned int power;               //      unsigned long power;
//    } hit;

    struct hit_struct{
        uint16_t key;
        uint16_t pfb_bin;
        uint16_t fft_bin;
        uint16_t power;
    } hit;

    struct hit_struct* hit_buf;
// time structure
    struct timeval timecode;
    const struct tm *timeptr;
    time_t now;
    char timebuf[81];               //new
    unsigned int time_start;
    int time_start_us;
    unsigned int time_stop;
    int time_stop_us;
    int time_diff_us, time_diff;
    float avg_time;
//spec structure
    struct spec_struct{
        struct timeval timecode;
        int spectra[COARSEBINS];
    } spec;
    struct spec_struct* spec_buf;

    U32            *ptr;
    U32            *ptr2;
     /* U32 defined in setispec.h */
    /* PAYLOADSIZE = 4096 defined in setispec.h */
/* malloc =   */
    U32        *buf = (U32 *)malloc(PAYLOADSIZE);
    ptr  = buf;
    ptr2 = buf;
    unsigned int prev_cnt = 0;
    unsigned int num_jump = 0;
    unsigned int num_good = 0;
    unsigned int max_spectra = INT_MAX;        // Max value
//values resettable in getarg
    int num_files = 1;  /*10*/          /* DEFAULT unless overridden in parameter call */
    int def_hits = 26;                  /* DEFAULT unless overridden by bee2.config file */
    unsigned int num_spectra = 540;     /* DEFAULT unless overridden in parameter call */
    int num_spectra_parm = 540;
    int limit = 0;
    int max_hits = 0;
/* end DEFS */

//START
// rearrange 5-18-2010
    limit = get_bee_config();
//    printf ("limit = %d\n",limit);
    if (limit != def_hits) {
        max_hits = limit;
    } else {
        max_hits = def_hits;
    }
    get_args(argc, argv, &num_files, &num_spectra_parm);

    printf("Maximum allowed hits = %d\n",max_hits);
    printf("Maximum number of spectra = %d\n",num_spectra_parm);
    printf("Maximum number of output files of each kind = %d\n",num_files);

     num_spectra = (unsigned int) num_spectra_parm;
     if (num_spectra == 0) {
         num_spectra = max_spectra;
         printf ("DO FOREVER - (%d spectra) \n",num_spectra);
     }
     if (num_files == 0) {
         num_files = 100000;
         printf ("%d files of %d spectra\n",num_files,num_spectra);
     }

    hit_buf = malloc(sizeof(hit));
    spec_buf = malloc(sizeof(spec));


//    get_bee_config();
    /* Try to open the socket */
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
        perror("??E - socket open failure");
        exit(1);
    }
    /* get local address info and populate sockaddr_in struct */
    recv_addr.sin_family      = AF_INET;               // host byte order
    recv_addr.sin_port        = htons(RECVPORT); // short, network byte order
    recv_addr.sin_addr.s_addr = INADDR_ANY;      // automatically fill with

/* memset =  */
    memset(recv_addr.sin_zero, '\0', sizeof recv_addr.sin_zero);

    /* bind socket file descriptor to local addr and port */
    if (bind(sockfd, (struct sockaddr *)&recv_addr, sizeof recv_addr) == -1) {
        perror("??E - bind socket error");
        exit(1);
    }

    addr_len = sizeof (send_addr);
    /* open the header, data (coarse) and hit fileis */
    for (j=0; j < num_files; j++) {
        file_full = 0;
//fopen HEADER FILE - normal
        sprintf((char *)hdr_file_str,
            (const char *)"/usr/local/datafiles/spc%05d.hdr",
            j);

        hdr_file = fopen(hdr_file_str,"w");
        if (hdr_file == NULL) {
            printf ("\n??E - fopen failed on  %s  \n", hdr_file_str);
            printf ("check file ownership and protections\n\n");
            exit(1);
        }

//fopen .crs  coarsebins FILE - normal
        sprintf((char *)data_file_str,
            (const char *)"/usr/local/datafiles/spc%05d.crs",
            j);
        data_file = fopen(data_file_str,"wb");
        if (data_file == NULL) {
            printf ("\n??E - fopen failed on  %s  \n", data_file_str);
            printf ("check file ownership and protections\n\n");
            exit(1);
        }

//fopen .hit FILE - normal
//        printf ("fopen data_file_str= %s\n", data_file_str);
        sprintf((char *)hit_file_str,
            (const char *)"/usr/local/datafiles/spc%05d.hit",
            j);
        hit_file = fopen(hit_file_str,"wb");
        if (hit_file == NULL) {
            printf ("\??E - nfopen failed on  %s  \n", hit_file_str);
            printf ("check file ownership and protections\n\n");
            exit(1);
        }
            /*   Read 'num_files' packets from the socket into 'buf'. Each packet
                consists of PAYLOADSIZE (= 4096) bytes or 512 U32 pairs. If there are no
                hits, each pair has a PFB channel average power.  A full spectrum
                consists of COARSEBINS (= 4096) bins, so 8 packets would comprise one
                spectrum.
                Spectra are produced at the rate one every 0.67 sec. Each spectrum
                is written as 16,400 bytes (16 bytes timecode + 4096 4-byte integers.
                So datafiles grow at the rate of 10.73 kB/s = 1.4 MB/min. */
        hit_ctr = 0;
        spectrum_ctr = -1;
        synced = 0;
//Write hdr
        gettimeofday(&timecode,NULL);
        time(&now);
        timeptr = localtime (&now);
        time_start = timecode.tv_sec;
        time_start_us = timecode.tv_usec;
        strftime(timebuf, 80, "%a %b %d %X %Z %Y",timeptr);
        printf ("STARTING TIME: %s\n",timebuf);

        fprintf(hdr_file,"STARTING TIMECODE_S  : %ld\n",timecode.tv_sec);
        fprintf(hdr_file,"STARTING TIMECODE_US : %ld\n",timecode.tv_usec);
        fprintf (hdr_file,"STARTING TIME        : %s\n",timebuf);
        fprintf(hdr_file,"%s \n",bee_config_buffer);
        fclose(hdr_file);
//DO FOREVER
        printf ("DO FOREVER\n");
            while (1) {

            maxvalue=0.0e0;
            /*        Receive one payload of data from the socket */
            if ((numbytes = recvfrom(sockfd,
                 buf,
                 PAYLOADSIZE ,
                 1,
                 (struct sockaddr *)&send_addr,
                               &addr_len))
                == -1) {
                perror("??E - recvfrom socket error");
                exit(1);
            }
            /* The structure data_vals is defined in setispec.h and consists
                of one unsigned int 'raw_data' and one unsigned int 'overflow_cntr'.
                We might need to worry about 'int' and U32 being the same. */
                /* data structures */
                //    struct data_vals{
                //        unsigned int raw_data;
                //        unsigned int overflow_cntr;
                //    };
                //    #define U32 unsigned int
                //    U32        *buf = (U32 *)malloc(PAYLOADSIZE);
            struct data_vals *data_ptr;                       /* set here */
            /* data_ptr is a pointer to data_vals structures in buf.
                It is incremented to advance through the data read in. Her
                it is initialized to the beginning of buf. */
            data_ptr = (struct data_vals *)buf;
            data_ptr_cnt = 0;
            /* looping over one payload (4096 U32s), 8 at a time */
            for (k=0; k<PAYLOADSIZE; k+=8) {
                /* Extract the indices and booleans */
                /* if you want to catch last bit of previous spectrum   */
                /* change to (k=0; k<(PAYLOADSIZE + 9); k+=8)          */
                U32 fields = ntohl(data_ptr->raw_data);
                /* slice is defined in casper.c
                    It takes 'width' bits from 'value' ignoring the lowest
                    'offset' bits.
                    'fields' has the following bit pattern:
                    EBBBOpp pppppppp pfffffff ffffffff
                    e - 1 event bit
                    b - 3 unused bits
                    o - 1 over-threshold bit
                    p - 12 bits pfb bin value
                    f - 15 bits fft bin value  */
                fft_bin = slice(fields,15,0);     /* extract bits 14- 0 */
                pfb_bin = slice(fields,12,15);    /* extract bits 26-15 */
//                    printf("pfb_bin = %d fft_bin = %d \n",pfb_bin,fft_bin);

                over_thresh = slice(fields,1,27); /* extract bit  27    */
                blank = slice(fields,3,28);       /* extract bits 30-28 */
                event = slice(fields,1,31);       /* extract bit  31    */
          /* extract the power data */
                pfb_fft_power = ntohl(data_ptr->overflow_cntr);
          /* Do nothing until beginning of first spectrum */
                if (fft_bin == 0 && pfb_bin == 0 && spectrum_ctr == -1) {
                  /* ready to start */
                    synced = 1;
                    spectrum_ctr = 0;
                    channel_ctr = -1;
                    gettimeofday(&timecode,NULL);
                    time_start = timecode.tv_sec;
                    time_start_us = timecode.tv_usec;
                    fprintf(stderr,"Synced up and ready to start\n");
//                    printf ("Synced up and ready to start\n");
                }
                if (synced == 1) {
                    if (fft_bin == 0) {
                        channel_ctr++;
/////                        printf ("Channel ctr = %d\n",channel_ctr);
                        //populate array spectra 
                        if (pfb_bin == channel_ctr) {
                            spectra[channel_ctr] = pfb_fft_power;
                        }
                    }  else {
                        //save the hits
                        //Dec 2009 - change power to 1000*decibels
//printf ("fft_bin > 0\n");
                        maxvalue = 1000*log10((double) pfb_fft_power);
                        log100power = (uint16_t) maxvalue;
                        hit.key = (uint16_t) spectrum_ctr;
                        hit.pfb_bin = (uint16_t) pfb_bin;
                        hit.fft_bin = (uint16_t) fft_bin;
//                        hit.sixtysix = t66;
//                        hit.hitctr =  hit_ctr;
//                        hit.power = pfb_fft_power;
                        hit.power = (uint16_t) log100power;
//printf ("log1000power = %d\n",log1000power);
/////printf ("before hit_buf[%d]\n",hit_ctr);
                        hit_buf[0] = hit;
/////printf ("after hit_buf[hit_ctr]\n");
                        // save maxpower and what hit counter it is in
                        if (pfb_fft_power > maxpower) {
                            maxpower = pfb_fft_power;
                            maxpower_hit_ctr = hit_ctr;
                            maxpower_spectrum = spectrum_ctr;
                            maxpower_pfb_bin = pfb_bin;
                            maxpower_fft_bin = fft_bin;
                        }
//                        printf ("write HIT_BUF  hit_ctr = %d  bytes =  %ld\n",hit_ctr,sizeof(hit));
                        fwrite( (const void *)hit_buf,
                            sizeof(hit),
                            1,
                            (FILE *) hit_file);

                        fflush((FILE *) hit_file);
//                        printf ("last hit_ctr = %d\n",hit_ctr);

                        hit_ctr++;
//                        printf ("Hit ctr = %d\n",hit_ctr);
                    } // end if (fft_bin == 0)
                    //if spectra is full, save it
                    if (channel_ctr == COARSEBINS-1) {
                        //rearange array
//                        printf ("rearrange\n");
                        for (l=0; l<COARSEBINS/2; l++) {
                            tmp=spectra[l];
                            spectra[l]=spectra[l+2048];
                            spec.spectra[l]=spectra[l+2048];
                            spectra[l+2048]=tmp;
                            spec.spectra[l+2048]=tmp;
                        }
                        //reset counter
                        channel_ctr=-1;
                        // increment spectrum counter
                        spectrum_ctr++;               /* WHERE IS THIS RESET? */
//                        printf ("Write to spectra and hits, spectrum_ctr = %d\n",spectrum_ctr);
                        /* fwrite to data_file (spc00000.crs)
                            size_t fwrite(const void *ptr,
                                size_t size,
                                size_t nitems,
                                FILE *stream);
                            The fwrite() function writes, from the array pointed to
                            by 'ptr', up to 'nitems' members whose size is specified
                            by 'size', to the stream pointed to by 'stream'. The
                            file-position indicator for the stream (if defined)
                            is advanced by the number of bytes successfully written.
                            If an error occurs, the resulting value of the
                            file-position indicator for the stream is
                            indeterminate. */
//                        printf("main: fwrite spectra = %d\n",temp_size);

//                        size_t sizewritten=fwrite((const void *)spectra,
//                            COARSEBINS*sizeof(int),
//                            1,
//                            (FILE *)data_file);
                        gettimeofday(&spec.timecode,NULL);
                /* fwrite to coarse_file (spc00000.crs)            */
                            fwrite(&timecode,
                            sizeof(timecode),
                            1,
                            (FILE *)data_file);

                        size_t sizewritten=fwrite((const void *)spectra,
                            COARSEBINS*sizeof(int),
                            1,
                            (FILE *)data_file);
                        if (sizewritten != 1) {
                            printf ("sizewritten = %d, on %s\n",(int) sizewritten, data_file_str);
                        }
                        fflush((FILE *)data_file);
//                        fprintf(stderr, "printing %ld bytes spectrum_ctr = %d\n", COARSEBINS*sizeof(int),spectrum_ctr);

                        if (spectrum_ctr == num_spectra) {
//                            close(sockfd);
//                            free(hit_buf);
                            spectrum_ctr = -1;
                            synced = 0;
//                            printf ("spectrum_ctr reset = -1\n");
                            file_full = 1;

                        }
                        if (file_full == 1) {
                            break;
                        }
/////printf ("after2\n");
                    } // end if (channel_ctr == COARSEBINS)
                    if (file_full == 1) {
                        break;
                    }
//printf ("after3\n");
                } // end if synced
                if( (pfb_bin - prev_cnt) > 1 && (pfb_bin - prev_cnt) != -4095) {
                    num_jump++;
                }
                prev_cnt = pfb_bin;
                data_ptr++;
                data_ptr_cnt++;
//                if (data_ptr_cnt == 512 && prev_cnt == 4085) {
//                    printf ("end if_synced: num_jump = %d, data_ptr_cnt = %d, prev_cnt = %d\n",
//                    num_jump, data_ptr_cnt, prev_cnt);
//                }
                if (file_full == 1) {
                    break;
                }
//printf ("after if (file_ful == 1\n");
            } // end for (k=0; k<PAYLOADSIZE; k+=8)

//printf ("end for (k=0\n");
            if (file_full == 1) {
               break;
            }

        } // end while(1)
// num_jump incremented
//END DO FOREVER

printf ("end while\n");
        if (num_jump == 0) {
            num_good++;             /* WHERE IS THIS RESET? */   /* below in else */
        } else {
            printf("Number of jumps detected = %d (1 is OK)\n", num_jump);
            num_jump = 0;
        }
        fclose(data_file);
        fclose(hit_file);
        gettimeofday(&timecode,NULL);
        time(&now);
        timeptr = localtime (&now);
        time_stop = timecode.tv_sec;
        time_stop_us = timecode.tv_usec;

        strftime(timebuf, 80, "%a %b %d %X %Z %Y",timeptr);
        printf ("ENDING TIME: %s\n",timebuf);

// reopen HEADER FILE
        hdr_file = fopen(hdr_file_str,"aw");
        if (hdr_file == NULL) {
            printf ("\n??E - fopen failed on appending %s  \n", hdr_file_str);
            printf ("check file ownership and protections\n\n");
            exit(1);
        }

        fprintf(hdr_file,"SYNC TIMECODE_S    : %d\n",time_start);
        fprintf(hdr_file,"SYNC TIMECODE_US   : %d\n",time_start_us);
        fprintf(hdr_file,"ENDING TIMECODE_S  : %ld\n",timecode.tv_sec);
        fprintf(hdr_file,"ENDING TIMECODE_US : %ld\n",timecode.tv_usec);
        fprintf (hdr_file,"ENDING TIME        : %s\n",timebuf);
        time_diff_us =  time_stop_us - time_start_us;
//        printf ("time_diff_us = %d\n",time_diff_us);
        time_diff =  time_stop - time_start;
        if (time_diff_us < 0) {
            time_diff--;
            time_diff_us = 1000000 + time_diff_us;
        }
        avg_time = time_diff + 10e-7*time_diff_us;
        avg_time = avg_time/num_spectra;
        maxvalue = 1000*log10((double) maxpower);
        log100power = (uint16_t) maxvalue;
        db_power = (float) log100power/100.;
        printf ("%d spectra: time_diff = %d sec, %d usec: (average = %f seconds)\n",
            num_spectra,time_diff,time_diff_us,avg_time);
//        fprintf(hdr_file,"%s \n",bee_config_buffer);
        fprintf(hdr_file,"%d spectra TIME DIFFERENCE = %d.%d seconds (average = %f seconds)\n",
            num_spectra,time_diff,time_diff_us,avg_time);
//Spectra are produced at the rate one every 0.67 sec
        fprintf(hdr_file,"Nominal spectra rate = 0.67 per sec\n");
        fprintf(hdr_file,"MAXPOWER = %d (%6.3f db) in spectrum = %d pfb_bin = %d fft_bin = %d at hit_ctr = %d\n",
            maxpower,db_power,maxpower_spectrum,maxpower_pfb_bin,maxpower_fft_bin,maxpower_hit_ctr);
        fprintf(hdr_file,"Number of jumps = %d number of good = %d\n", num_jump,num_good);
        fclose(hdr_file);

        printf("Maxpower = %d (%6.3f db) in spectrum = %d, pfb_bin = %d, fft_bin = %d, at hit_ctr = %d\n",
            maxpower,db_power,maxpower_spectrum,maxpower_pfb_bin,maxpower_fft_bin,maxpower_hit_ctr);

    }  //  for (j=0; j < num_files; j++)

    if (spectrum_ctr == num_spectra) {
        close(sockfd);
        free(hit_buf);
        free(spec_buf);
    }
    return 0;
}


