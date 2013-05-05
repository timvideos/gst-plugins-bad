#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <unistd.h>
#include <math.h>

//#define BAUDRATE B38400
#define BAUDRATE B9600
#define _POSIX_SOURCE 1         /* POSIX compliant source */
#define FALSE 0
#define TRUE 1

volatile int STOP = FALSE;

int
main (int argc, char **argv)
{
  struct termios oldtio1, newtio1;
  struct termios oldtio2, newtio2;
  int fd1, fd2, n;
  char buf[255];

  fd_set readfs;
  int maxfd;

  (void) n;

  if (argc != 3) {
    return -1;
  }

  fd1 = open (argv[1], O_RDWR | O_NOCTTY);
  if (fd1 < 0) {
    perror (argv[1]);
    exit (-1);
  }

  fd2 = open (argv[2], O_RDWR | O_NOCTTY);
  if (fd2 < 0) {
    perror (argv[2]);
    exit (-1);
  }

  {
    tcgetattr (fd1, &oldtio1);
    bzero (&newtio1, sizeof (newtio1));
    /* 
       BAUDRATE: Set bps rate. You could also use cfsetispeed and cfsetospeed.
       CRTSCTS : output hardware flow control (only used if the cable has
       all necessary lines. See sect. 7 of Serial-HOWTO)
       CS8     : 8n1 (8bit,no parity,1 stopbit)
       CLOCAL  : local connection, no modem contol
       CREAD   : enable receiving characters
     */
    //newtio1.c_cflag = BAUDRATE | CRTSCTS | CS8 | CLOCAL | CREAD;
    newtio1.c_cflag = BAUDRATE | CS8;

    /*
       IGNPAR  : ignore bytes with parity errors
       ICRNL   : map CR to NL (otherwise a CR input on the other computer
       will not terminate input)
       otherwise make device raw (no other input processing)
     */
    newtio1.c_iflag = IGNPAR | ICRNL;
    newtio1.c_oflag = 0;

    /*
       Raw output.
     */
    newtio1.c_oflag = 0;

    /*
       ICANON  : enable canonical input
       disable all echo functionality, and don't send signals to calling program
     */
    //newtio1.c_lflag = ICANON;

    /* 
       initialize all control characters 
       default values can be found in /usr/include/termios.h, and are given
       in the comments, but we don't need them here
     */
#if 0
    newtio.c_cc[VINTR] = 0;     /* Ctrl-c */
    newtio.c_cc[VQUIT] = 0;     /* Ctrl-\ */
    newtio.c_cc[VERASE] = 0;    /* del */
    newtio.c_cc[VKILL] = 0;     /* @ */
    newtio.c_cc[VEOF] = 4;      /* Ctrl-d */
    newtio.c_cc[VTIME] = 0;     /* inter-character timer unused */
    newtio.c_cc[VMIN] = 1;      /* blocking read until 1 character arrives */
    newtio.c_cc[VSWTC] = 0;     /* '\0' */
    newtio.c_cc[VSTART] = 0;    /* Ctrl-q */
    newtio.c_cc[VSTOP] = 0;     /* Ctrl-s */
    newtio.c_cc[VSUSP] = 0;     /* Ctrl-z */
    newtio.c_cc[VEOL] = 0;      /* '\0' */
    newtio.c_cc[VREPRINT] = 0;  /* Ctrl-r */
    newtio.c_cc[VDISCARD] = 0;  /* Ctrl-u */
    newtio.c_cc[VWERASE] = 0;   /* Ctrl-w */
    newtio.c_cc[VLNEXT] = 0;    /* Ctrl-v */
    newtio.c_cc[VEOL2] = 0;     /* '\0' */
#endif

    tcflush (fd1, TCIFLUSH);
    tcsetattr (fd1, TCSANOW, &newtio1);
  }
  {
    tcgetattr (fd2, &oldtio2);
    bzero (&newtio2, sizeof (newtio2));
    //newtio2.c_cflag = BAUDRATE | CRTSCTS | CS8 | CLOCAL | CREAD;
    newtio2.c_cflag = BAUDRATE | CS8;
    newtio2.c_iflag = IGNPAR | ICRNL;
    newtio2.c_oflag = 0;
    //newtio2.c_lflag = ICANON;
    tcflush (fd2, TCIFLUSH);
    tcsetattr (fd2, TCSANOW, &newtio2);
  }

  {
    printf ("send command: QSV\n");
    bzero (buf, sizeof (buf));
    sprintf (buf, "\x02QSV\x03");
    write (fd1, buf, strlen (buf));
    tcdrain (fd1);

    usleep (5000);

    bzero (buf, sizeof (buf));
    read (fd2, buf, sizeof (buf));
    printf ("reply: %s\n", buf);
  }
  {
    printf ("send command\n");
    bzero (buf, sizeof (buf));
    sprintf (buf, "\x23On\x0D");
    write (fd1, buf, strlen (buf));
    tcdrain (fd1);

    usleep (5000);

    bzero (buf, sizeof (buf));
    read (fd2, buf, sizeof (buf));
    printf ("reply: %s\n", buf);
  }

  maxfd = (fd1 < fd2 ? fd2 : fd1) + 1;
  while (0) {
    FD_SET (fd1, &readfs);
    FD_SET (fd2, &readfs);
    select (maxfd, &readfs, NULL, NULL, NULL);
    if (FD_ISSET (fd1, &readfs)) {
      printf ("send command\n");
      bzero (buf, sizeof (buf));
      sprintf (buf, "\x23O2\x0D");
      write (fd1, buf, strlen (buf));
    }
    if (FD_ISSET (fd2, &readfs)) {
      bzero (buf, sizeof (buf));
      read (fd2, buf, sizeof (buf));
      printf ("reply: %s\n", buf);
      break;
    }
  }

  printf ("commands sent, restoring attributes\n");

  tcsetattr (fd1, TCSANOW, &oldtio1);
  close (fd1);
  tcsetattr (fd2, TCSANOW, &oldtio2);
  close (fd2);

  printf ("done\n");
  return 0;
}
