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
    indicates the packet type: '!' simply writes the rest of the
    packet to the mood lamp, '#RRGGBB' writes a color.
*/

#include <iostream>
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

enum moodpd_pkttype
{
    MOODPD_RAWMSG= '!',
    MOODPD_COLOR= '#',
};

// moodpd packet structure. it's mostly just a string.
struct moodpd_packet
{
    uint32_t magic;     // MOODPD_MAGIC
    char type;          // set to MOODPD_RAWMSG
    char message[];     // message to send to mood lamp.
};

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

    fprintf(stderr,"init_serialport: opening port %s\n", devname);

    fd = open(devname, O_RDWR | O_NOCTTY | O_NDELAY);
    if (fd == -1)  {
        perror("init_serialport: Unable to open port ");
        return -1;
    }

    if (tcgetattr(fd, &toptions) < 0) {
        perror("init_serialport: Couldn't get term attributes");
        return -1;
    }

    // return fd;  // todo: set serial parameters correctly

    cfsetispeed(&toptions, B230400);
    cfsetospeed(&toptions, B230400);

    // #set up raw mode / no echo / binary
    toptions.c_cflag|= CLOCAL|CREAD;
    toptions.c_lflag&= ~(ICANON|ECHO|ECHOE|ECHOK|ECHONL|ISIG|IEXTEN);

    // #netbsd workaround for Erk
    toptions.c_lflag&= ~(ECHOCTL|ECHOKE);

//        oflag &= ~(TERMIOS.OPOST)
//        iflag &= ~(TERMIOS.INLCR|TERMIOS.IGNCR|TERMIOS.ICRNL|TERMIOS.IGNBRK)
//        if hasattr(TERMIOS, 'IUCLC'):
//            iflag &= ~TERMIOS.IUCLC
//        if hasattr(TERMIOS, 'PARMRK'):
//            iflag &= ~TERMIOS.PARMRK
    toptions.c_oflag &= ~(OPOST);
    toptions.c_iflag &= ~(INLCR|IGNCR|ICRNL|IGNBRK);
    // #if hasattr(TERMIOS, 'IUCLC'):
    toptions.c_iflag &= ~IUCLC;
    // #if hasattr(TERMIOS, 'PARMRK'):
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

    if( tcsetattr(fd, TCSANOW, &toptions) < 0) {
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

//    FILE *serfile= fdopen(serfd, "rw");

//  write(3, "acI\1\2\2ab", 8)              = 8
//  write(3, "acW\0ab", 6)                  = 6

    char init0[]= "acI\1\2\2ab";
    char init1[]= "acW\0ab";
    write(serfd, init0, sizeof(init0)-1);
    write(serfd, init1, sizeof(init1)-1);


    fd_set readset;
    while(true)
    {
        FD_ZERO(&readset);
        FD_SET(sock, &readset);

        // we could just blocking recvfrom(), but may want to poll more fds later.
        if(select(sock+1, &readset, 0, 0, 0)<0)
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
            buf[sz]= 0;
            moodpd_packet *p= (moodpd_packet*)buf;
            if(p->magic != MOODPD_MAGIC || (unsigned)sz<sizeof(moodpd_packet))
            {
                fprintf(stderr, "received malformed packet from %s.\n", inet_ntoa(sa_from.sin_addr));
                continue;
            }

            int msgsize= sz-offsetof(moodpd_packet, message);
            switch(p->type)
            {
                case MOODPD_RAWMSG:
                {
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
                    printf("color: %d %d %d\n", r, g, b);
                    fflush(stdout);
                    //"acP\x02C\xRR\xGG\xBBab"
                    char ch[64];
                    sprintf(ch, "acP\2C%c%c%cab", r, g, b);
                    write(serfd, ch, 10);
//                    tcflush(serfd, TCOFLUSH);
//                    tcflush(serfd, TCIFLUSH);
                    break;
                }
                default:
                    fprintf(stderr, "unknown packet type 0x%02X.\n", p->type);
                    break;
            }
        }
    }

    return 0;
}
