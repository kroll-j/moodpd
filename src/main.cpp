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

        !...        send raw command bytes to mood lamp (only if enabled)

        old muccc-style commands (compile-time option, currently disabled):
            BVV         sets global brightness to VV (CMD_SET_BRIGHTNESS)
            FRRGGBBTTTT fade to color RRGGBB in TTTT milliseconds (CMD_FADEMS)
            P           cycle pause state (CMD_PAUSE)
            X           CMD_POWER

    run moodpd -h for help, press '?' in interactive mode for help on keys
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
#include <deque>
#include <vector>
#include <string>
#include <stdarg.h>
#include <errno.h>
#include <poll.h>
#include <libgen.h>
#include <iostream>

#include <oscpkt.hh>
#include <udp.hh>

using namespace std;

// use muccc-style commands. currently disabled, new firmware uses other cmds. 
// #define MUCPROTOCOL

#include "cmd_handler.h"
#include "utils.h"

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

uint32_t logMask= 1<<LOG_ERROR;


class SerialIO: public NonblockWriter
{
	public:
		SerialIO()
		{ }

        bool open(const char *devname= "/dev/ttyUSB0")
        {
			NonblockWriter::setFd(openSerial(devname));
            return getFd()>=0;
        }

        void writeFailed(int _errno)
        {
            fail(strerror(_errno));
        }

        void writeCommand(const char *commandBytes, int length)
        {
            if(length<=0) fail("writeCommand");
#ifdef MUCPROTOCOL
            write("acP\2", 4);
            write(commandBytes, length);
            write("ab", 2);
#else
            write(commandBytes, length);
#endif
            if(logMask&(1<<LOG_INFO))
            {
                flog(LOG_INFO, "writeCommand: '");
                for(int i= 0; i<length; i++)
                    fprintf(stderr, (isalpha(commandBytes[i])? "%c": "\\x%02X"), commandBytes[i]);
                fprintf(stderr, "' (buffer size: %d)\n", getWritebufferSize());
            }
        }

        void writeCommandF(const char *fmt, ...)
        {
            char ch[1024];
            va_list ap;
            va_start(ap, fmt);
            int nbytes= vsnprintf(ch, 1023, fmt, ap);
            va_end(ap);
            if(nbytes>0) writeCommand(ch, nbytes);
            else fail("vsnprintf");
        }

	private:
		int openSerial(const char* devname= "/dev/ttyUSB0")
		{
			struct termios toptions;
			memset(&toptions, 0, sizeof(toptions));
			int fd;

			flog(LOG_INFO, "openSerial: opening port %s\n", devname);

			fd = ::open(devname, O_RDWR | O_NOCTTY | O_NDELAY);
			if (fd == -1)  {
				logerror("openSerial: open");
				return -1;
			}
            
            if(!isatty(fd))
                // don't try to setup anything if it's not a tty. 
                // this allows redirecting stuff for testing.
                return fd;

			if (tcgetattr(fd, &toptions) < 0) {
				logerror("openSerial: Couldn't get term attributes");
                close(fd);
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
				logerror("openSerial: Couldn't set term attributes");
                close(fd);
				return -1;
			}

			return fd;
		}
};

void setLineOrientedStdin(bool restore= false)
{
    static termios oldSettings;
    termios newSettings;

    if(!restore)
    {
        tcgetattr(STDIN_FILENO, &oldSettings);
        newSettings= oldSettings;
        newSettings.c_lflag&= ~(ICANON|ECHO);  // disable canonical (line-oriented) mode and echoing
        newSettings.c_cc[VTIME]= 1;     // timeout (tenths of seconds)
        newSettings.c_cc[VMIN]= 0;      // minimum number of characters to buffer
        tcsetattr(STDIN_FILENO, TCSANOW, &newSettings);
    }
    else
        tcsetattr(STDIN_FILENO, TCSANOW, &oldSettings);
}

void atexitfn()
{
    setLineOrientedStdin(true);
}

void printHelp(char *comm)
{
    printf("use: %s [options]\n", comm);
    printf("options:\n"
           "    -h              print this text.\n"
           "    -r              allow raw mode (default off)\n"
           "    -l FLAGS        set logging flags\n"
           "                        e: log error messages (default)\n"
           "                        i: log error and informational messages\n"
           "                        q: quiet mode, don't log anything\n"
           "                    flags can be combined.\n"
           "    -d              daemonize\n"
           "    -t TTYNAME      set moodlamp tty [/dev/ttyUSB0]\n"
           "\n");
}

// main app class
class moodpd
{
    public:
        moodpd(int argc, char *argv[]): allowRawMode(false)
        {
            string lamptty= "/dev/ttyUSB0";
            
            // parse the command line.
            char opt;
            while( (opt= getopt(argc, argv, "hl:dt:"))!=-1 )
                switch(opt)
                {
                    case '?':
                        printHelp(argv[0]);
                        exit(1);
                    case 'h':
                        printHelp(argv[0]);
                        exit(0);
                    case 'l':
                        for(int i= 0; optarg[i]; i++) switch(optarg[i])
                        {
                            case 'i':
                                logMask|= (1<<LOG_INFO);
                            case 'e':
                                logMask|= (1<<LOG_ERROR);
                                break;
                            case 'q':
                                logMask= 0;
                                break;
                            default:
                                printf("unknown logging flag -- '%c'\n", optarg[i]);
                                printHelp(argv[0]);
                                exit(1);
                        }
                        break;
                    case 'd':
                        daemonize();
                        break;
                    case 't':
                        if(optarg)
                            lamptty= optarg;
                        else
                        {
                            printHelp(argv[0]);
                            exit(1);
                        }
                        break;
                }

            setLineOrientedStdin();
            atexit(atexitfn);

            sock= socket(AF_INET, SOCK_DGRAM, 0);
            if(sock<0) fail("socket");

            struct sockaddr_in sa;
            memset(&sa, 0, sizeof(sa));
            sa.sin_family= AF_INET;
            sa.sin_addr.s_addr= htonl(INADDR_ANY);
            sa.sin_port= htons(DEFAULT_PORT);
            if(bind(sock, (sockaddr*)&sa, sizeof(sa))<0)
                fail("bind");

            if(!serial.open(lamptty.c_str())) fail("openSerial");
            
            if(!oscSocket.bindTo(DEFAULT_PORT+1, oscpkt::UdpSocket::OPTION_UNSPEC)) 
                fail("osc socket: bindTo() failed");
            
            

#ifdef MUCPROTOCOL
            // write some magic undocumented initialization bytes...
            char init0[]= "acI\1\2\2ab";
            char init1[]= "acW\0ab";
            serial.write(init0, sizeof(init0)-1);
            serial.write(init1, sizeof(init1)-1);
#endif
        }

        void run()
        {
            flog(LOG_INFO, "entering main loop.\n");

            fd_set readset;
            while(true)
            {
                vector<pollfd> pollfds;

                pollfds.push_back( (pollfd){ sock, POLLIN, 0 } );
                pollfds.push_back( (pollfd){ serial.getFd(), (isatty(serial.getFd())? POLLIN: 0) | (serial.writeBufferEmpty()? 0: POLLOUT), 0 } );
                pollfds.push_back( (pollfd){ STDIN_FILENO, POLLIN, 0 } );
                pollfds.push_back( (pollfd){ oscSocket.socketHandle(), POLLIN, 0 } );

                if(poll(&pollfds.front(), pollfds.size(), -1)<0)
                    fail("poll");
 
                for(int i= 0; i<pollfds.size(); i++)
                {
                    pollfd &pfd= pollfds[i];
                    if(pfd.revents & (POLLERR|POLLRDHUP|POLLHUP|POLLNVAL))
                        flog(LOG_CRIT, "poll: %sfd went bad.\n", pfd.fd==sock? "socket ": pfd.fd==serial.getFd()? "serial ": ""),
                        exit(1);
                    if(pfd.fd==sock)
                    {
                        if(!pfd.revents&POLLIN) continue;
                        sockaddr_in sa_from;
                        socklen_t sa_len= sizeof(sa_from);
                        char buf[MOODPD_MAXPACKETSIZE+1];
                        memset(&sa_from, 0, sizeof(sa_from));
                        ssize_t sz= recvfrom(sock, buf, sizeof(buf)-1, 0, (sockaddr*)&sa_from, &sa_len);
                        if(sz<=0)
                        {
                            logerror("recvfrom");
                            continue;
                        }
                        buf[sz]= 0; // zero-terminate message string
                        moodpd_packet *p= (moodpd_packet*)buf;
                        if(p->magic != MOODPD_MAGIC || (unsigned)sz<=sizeof(moodpd_packet))
                        {
                            flog(LOG_ERROR, "received malformed packet from %s.\n", inet_ntoa(sa_from.sin_addr));
                            continue;
                        }
                        int msgsize= sz-offsetof(moodpd_packet, message);
                        parseMessage(p->type, p->message, msgsize);
                    }
                    else if(pfd.fd==serial.getFd())
                    {
                        if(pfd.revents&POLLIN)
                        {
                            char txt[1024];
                            int n= read(serial.getFd(), txt, 1023);
                            if(logMask & (1<<LOG_INFO))
                            {
                                if(n<=0) continue; // read error
                                txt[n]= 0;
                                flog(LOG_INFO, "the mood lamp says: '");
                                fflush(stderr);
                                n= write(STDERR_FILENO, txt, n);
                                fprintf(stderr, "'\n");
                            }
                        }
                        if(pfd.revents&POLLOUT)
                            flog(LOG_INFO, "serial ready for writing. buffer size: %d\n", serial.getWritebufferSize()),
                            serial.flush();
                    }
                    else if(pfd.fd==STDIN_FILENO)
                    {
                        if(!pfd.revents&POLLIN) continue;
                        char c;
                        ssize_t s= read(STDIN_FILENO, &c, 1);
                        if(s!=1) continue;
                        switch(c)
                        {
                            case '?':
                                printf( "KEYS:\n"
                                        "\t?\tshow this text\n"
                                        "\tv\tset verbosity\n"
                                        "\tr\tallow raw mode on/off\n");
                                break;
                            case 'v':
                                if(!logMask) { logMask|= (1<<LOG_ERROR); puts("verbosity: errors only"); }
                                else if(logMask&(1<<LOG_INFO)) { logMask= 0; puts("verbosity: quiet"); }
                                else if(logMask&(1<<LOG_ERROR)) { logMask|= (1<<LOG_INFO); puts("verbosity: errors+info"); }
                                break;
                            case 'r':
                                allowRawMode^= 1;
                                puts(allowRawMode? "allow raw mode ON": "allow raw mode OFF");
                                break;
                        }
                    }
                    else if(pfd.fd==oscSocket.socketHandle())
                    {
                        if(!pfd.revents&POLLIN) continue;
                        flog(LOG_INFO, "OSC packet\n");
                        if(oscSocket.receiveNextPacket(0))
                        {
                            oscpkt::PacketReader pr;
                            oscpkt::Message *msg;
                            pr.init(oscSocket.packetData(), oscSocket.packetSize());
                            while(pr.isOk() && (msg = pr.popMessage()) != 0)
                            {
                                int r, g, b;
                                if(msg->match("/moodpd/lamps/00/rgb")
                                    .popInt32(r)
                                    .popInt32(g)
                                    .popInt32(b)
                                    .isOkNoMoreArgs())
                                {
                                    r= min(255, max(r, 0));
                                    g= min(255, max(g, 0));
                                    b= min(255, max(b, 0));
                                    flog(LOG_INFO, "osc: red %d, green %d, blue %d\n", r, g, b);
                                    serial.writeCommandF("i%02x%02x%02x\n", r, g, b);
                                }
                                else if(msg->match("/ori") // andOSC android app thingy
                                    .popInt32(r)
                                    .popInt32(g)
                                    .popInt32(b)
                                    .isOkNoMoreArgs())
                                {
                                    flog(LOG_INFO, "andOSC orientation: %d, %d, %d\n", r, g, b);
                                    r= (r+180)%360*255/360;
                                    g= (g+180)%360*255/360;
                                    b= (b+180)%360*255/360;
                                    serial.writeCommandF("i%02x%02x%02x\n", r, g, b);
                                }
                            }
                        }
                    }
                }
            }
        }

        void parseMessage(char type, char *message, int msgsize)
        {
            char ch[64];
            switch(type)
            {
                case MOODPD_RAWMSG:
                {
                    if(!allowRawMode)
                    { flog(LOG_INFO, "raw message rejected.\n"); return; }
                    serial.write(message, msgsize);
                    if(logMask&(1<<LOG_INFO))
                    {
                        flog(LOG_INFO, "raw message: ");
                        for(int i= 0; i<msgsize; i++)
                            fprintf(stderr, "%02X ", (uint8_t)(message[i]));
                        fprintf(stderr, "\n");
                    }
                    break;
                }
                case MOODPD_COLOR:
                {
                    chomp(message);
                    int r= 0, g= 0, b= 0;
                    int matches= sscanf(message, "%02x%02x%02x", &r, &g, &b);
                    if(matches!=3)
                    {
                        flog(LOG_ERROR, "bad color string %s\n", message);
                        break;
                    }
#ifdef MUCPROTOCOL
                    serial.writeCommandF("C%c%c%c", r, g, b);
#else
                    serial.writeCommandF("i%02x%02x%02x\n", r, g, b);
#endif
                    break;
                }
#ifdef MUCPROTOCOL
                case MOODPD_SETBRIGHTNESS:
                {
                    chomp(message);
                    msgsize= strlen(message);
                    if(msgsize!=2)
                    {
                        flog(LOG_ERROR, "bad brightness string %s\n", message);
                        break;
                    }
                    int b;
                    sscanf(message, "%02x", &b);
                    serial.writeCommandF("%c%c", CMD_SET_BRIGHTNESS, b);
                    break;
                }
                case MOODPD_FADEMS:
                {
                    chomp(message);
                    msgsize= strlen(message);
                    if(msgsize!=10)
                    {
                        flog(LOG_ERROR, "bad fade parameters %s\n", message);
                        break;
                    }
                    int r, g, b, time;
                    sscanf(message, "%02x%02x%02x%04x", &r, &g, &b, &time);
                    serial.writeCommandF("%c%c%c%c%c%c", CMD_FADEMS, r,g,b, (time>>8)&0xff, time&0xff);
                    break;
                }
                case MOODPD_PAUSE:
                {
                    serial.writeCommandF("%c", CMD_PAUSE);
                    break;
                }
                case MOODPD_POWER:
                {
                    serial.writeCommandF("%c", CMD_POWER);
                    break;
                }
#endif // MUCPROTOCOL
                default:
                    flog(LOG_ERROR, "unknown packet type 0x%02X.\n", type);
                    break;
            }
        }

    private:
        bool allowRawMode;
        int sock;
        SerialIO serial;
        oscpkt::UdpSocket oscSocket;

	void daemonize()
	{
		int i= fork();
		if (i<0) exit(1); /* fork error */
		if (i>0) exit(0); /* parent exits */
		/* child (daemon) continues */
	}
};


int main(int argc, char *argv[])
{
    moodpd app(argc, argv);
    app.run();
    return 0;
}
