/*
    this is moodpd, a hack to forward udp packets to a muccc-style mood lamp.
    copyleft 2011, johannes kroll.

    to use, send one command per packet:
        int sock= socket(AF_INET, SOCK_DGRAM, 0);
        char buf[1024];
        snprintf(buf, sizeof(buf), "m00d#%02X%02X%02X", red, green, blue);
        sendto(sock, buf, strlen(buf), address...);

    from the command line:
        $ socat - UDP4-DATAGRAM:localhost:4242,
        m00d#104080
        m00d#f0f0f0
        ...

    packets not starting with 'm00d' are discarded. the next char
    indicates the packet type.
        #RRGGBB     writes a color (CMD_SET_COLOR)

        BVV         sets global brightness to VV (CMD_SET_BRIGHTNESS)
        FRRGGBBTTTT fade to color RRGGBB in TTTT milliseconds (CMD_FADEMS)
        P           cycle pause state (CMD_PAUSE)
        X           CMD_POWER
*/

#include <cstdio>
#include <cstdlib>
#include <memory.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>

#include "cmd_handler.h"

#ifndef max
#define max(a, b) ((a)>(b)? (a): (b))
#define min(a, b) ((a)<(b)? (a): (b))
#endif

enum moodpd_pkttype
{
    MOODPD_RAWMSG= '!',
    MOODPD_COLOR= '#',
    MOODPD_SETBRIGHTNESS= 'B',
    MOODPD_FADEMS= 'F',
    MOODPD_PAUSE= 'P',
    MOODPD_POWER= 'X',
};

// moodpd packet structure.
struct moodpd_packet
{
    uint32_t magic;     // MOODPD_MAGIC
    char type;          // moodpd_pkttype
    char message[];     // message to send to mood lamp.
} __attribute__((packed)); // to prevent padding at the end of the structure

// assume little-endian.
#define MOODPD_MAGIC    (*(uint32_t*)"m00d")

#define MOODPD_MAXPACKETSIZE    1024    // don't send packets larger than this.
#define DEFAULT_PORT 4242

using namespace std;


void fail(const char *msg= "")
{
    perror(msg);
    exit(1);
}


inline void chomp(char *line) { int n; while( (n= strlen(line)) && strchr("\r\n", line[n-1])) line[n-1]= 0; }


int openSerial(const char* devname= "/dev/ttyUSB0")
{
    struct termios toptions;
    memset(&toptions, 0, sizeof(toptions));
    int fd;

    fprintf(stderr, "init_serialport: opening port %s\n", devname);

    fd = open(devname, O_RDWR | O_NOCTTY | O_NDELAY);
    if (fd == -1)  {
        perror("init_serialport: Unable to open port ");
        return -1;
    }

    if (tcgetattr(fd, &toptions) < 0) {
        perror("init_serialport: Couldn't get term attributes");
        return -1;
    }

    cfsetispeed(&toptions, B230400);
    cfsetospeed(&toptions, B230400);

    // #set up raw mode / no echo / binary
    toptions.c_cflag|= CLOCAL|CREAD;
    toptions.c_lflag&= ~(ICANON|ECHO|ECHOE|ECHOK|ECHONL|ISIG|IEXTEN);

    // #netbsd workaround for Erk
    toptions.c_lflag&= ~(ECHOCTL|ECHOKE);

    toptions.c_oflag &= ~(OPOST);
    toptions.c_iflag &= ~(INLCR|IGNCR|ICRNL|IGNBRK);
    toptions.c_iflag &= ~IUCLC;
    toptions.c_iflag &= ~PARMRK;

    // char len: 8
    toptions.c_cflag &= ~CSIZE;
    toptions.c_cflag |= CS8;

    // #setup stopbits (1)
    toptions.c_cflag &= ~(CSTOPB);

    // #setup parity(none)
    toptions.c_iflag &= ~(INPCK|ISTRIP);
    toptions.c_cflag &= ~(PARENB|PARODD);

    // #setup flow control
    // #xonxoff
    toptions.c_iflag &= ~(IXON|IXOFF|IXANY);

    // #rtscts
    toptions.c_cflag &= ~(CRTSCTS);

    // #buffer
    // #vmin "minimal number of characters to be read. = for non blocking"
    toptions.c_cc[VMIN]= 0;
    // #vtime
    toptions.c_cc[VTIME]= 0;

    if( tcsetattr(fd, TCSANOW, &toptions) < 0)
    {
        perror("init_serialport: Couldn't set term attributes");
        return -1;
    }

    return fd;
}


int main()
{
    int sock= socket(AF_INET, SOCK_DGRAM, 0);
    if(sock<0) fail("socket");

    struct sockaddr_in sa;
    memset(&sa, 0, sizeof(sa));
    sa.sin_family= AF_INET;
    sa.sin_addr.s_addr= htonl(INADDR_ANY);
    sa.sin_port= htons(DEFAULT_PORT);
    if(bind(sock, (sockaddr*)&sa, sizeof(sa))<0)
        fail("bind");

    int serfd= openSerial();
    if(serfd<=0) exit(1);

    // write some magic undocumented initialization bytes...
    char init0[]= "acI\1\2\2ab";
    char init1[]= "acW\0ab";
    write(serfd, init0, sizeof(init0)-1);
    write(serfd, init1, sizeof(init1)-1);

    puts("entering main loop.");

    fd_set readset;
    while(true)
    {
        FD_ZERO(&readset);
        FD_SET(sock, &readset);
        FD_SET(serfd, &readset);

        // we could just blocking recvfrom(), but may want to poll more fds later.
        if(select(max(sock, serfd)+1, &readset, 0, 0, 0)<0)
        { perror("select"); continue; }

        if(FD_ISSET(sock, &readset))
        {
            sockaddr_in sa_from;
            socklen_t sa_len= sizeof(sa_from);
            char buf[MOODPD_MAXPACKETSIZE+1];
            memset(&sa_from, 0, sizeof(sa_from));
            ssize_t sz= recvfrom(sock, buf, sizeof(buf)-1, 0, (sockaddr*)&sa_from, &sa_len);
            if(sz<=0)
            {
                perror("recvfrom");
                continue;
            }
            buf[sz]= 0; // zero-terminate message string
            moodpd_packet *p= (moodpd_packet*)buf;
            if(p->magic != MOODPD_MAGIC || (unsigned)sz<=sizeof(moodpd_packet))
            {
                fprintf(stderr, "received malformed packet from %s.\n", inet_ntoa(sa_from.sin_addr));
                continue;
            }

            char ch[64];
            int msgsize= sz-offsetof(moodpd_packet, message);
            switch(p->type)
            {
                case MOODPD_RAWMSG:
                {
                    break; // xxx disabled for now (dangerous)
                    write(serfd, p->message, msgsize);
                    for(int i= 0; i<msgsize; i++)
                        printf("%02X ", p->message[i]);
                    puts("");
                    break;
                }
                case MOODPD_COLOR:
                {
                    chomp(p->message);
                    msgsize= strlen(p->message);
                    if(msgsize!=6)
                    {
                        fprintf(stderr, "bad color string %s\n", p->message);
                        break;
                    }
                    int r= 0, g= 0, b= 0;
                    sscanf(p->message, "%02x%02x%02x", &r, &g, &b);
                    sprintf(ch, "acP\2C%c%c%cab", r, g, b);
                    write(serfd, ch, 10);
                    break;
                }
                case MOODPD_SETBRIGHTNESS:
                {
                    chomp(p->message);
                    msgsize= strlen(p->message);
                    if(msgsize!=2)
                    {
                        fprintf(stderr, "bad brightness string %s\n", p->message);
                        break;
                    }
                    int b;
                    sscanf(p->message, "%02x", &b);
                    sprintf(ch, "ac%c%cab", CMD_SET_BRIGHTNESS, b);
                    write(serfd, ch, 6);
                    break;
                }
                case MOODPD_FADEMS:
                {
                    chomp(p->message);
                    msgsize= strlen(p->message);
                    if(msgsize!=10)
                    {
                        fprintf(stderr, "bad fade parameters %s\n", p->message);
                        break;
                    }
                    int r, g, b, time;
                    sscanf(p->message, "%02x%02x%02x%04x", &r, &g, &b, &time);
                    sprintf(ch, "ac%c%c%c%c%c%cab", CMD_FADEMS, r,g,b, (time>>8)&0xff, time&0xff);
                    write(serfd, ch, 10);
                    break;
                }
                case MOODPD_PAUSE:
                {
                    sprintf(ch, "ac%cab", CMD_PAUSE);
                    write(serfd, ch, 5);
                    break;
                }
                case MOODPD_POWER:
                {
                    sprintf(ch, "ac%cab", CMD_POWER);
                    write(serfd, ch, 5);
                    break;
                }
                default:
                    fprintf(stderr, "unknown packet type 0x%02X.\n", p->type);
                    break;
            }
        }
        if(FD_ISSET(serfd, &readset))
        {
            char txt[1024];
            int n= read(serfd, txt, 1023);
//            if(n<=0) continue; // xxx
//            txt[n]= 0;
//            printf("the mood lamp says: '");
//            fflush(stdout);
//            write(STDOUT_FILENO, txt, n);
//            puts("'");
        }
    }

    return 0;
}
