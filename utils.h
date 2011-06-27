#ifndef UTILS_H
#define UTILS_H

#ifndef max
#define max(a, b) ((a)>(b)? (a): (b))
#define min(a, b) ((a)<(b)? (a): (b))
#endif




inline void chomp(char *line) { int n; while( (n= strlen(line)) && strchr("\r\n", line[n-1])) line[n-1]= 0; }



// log levels for flog(). LOG_CRIT is always printed, other levels can be individually enabled on the command line.
enum Loglevel
{
    LOG_INFO,
    LOG_ERROR,
    LOG_CRIT
};


extern uint32_t logMask;

inline void flog(Loglevel level, const char *fmt, ...)
{
    if( !(logMask & (1<<level)) && level!=LOG_CRIT ) return;

    char timeStr[200];
    time_t t;
    struct tm *tmp;

    t= time(0);
    tmp= localtime(&t);
    if(!tmp)
        strcpy(timeStr, "localtime failed");
    else if(strftime(timeStr, sizeof(timeStr), "%F %H:%M.%S", tmp)==0)
        strcpy(timeStr, "strftime returned 0");

    fprintf(stderr, "[%s] ", timeStr);
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
}

#define logerror(str)   \
    flog(LOG_ERROR, "%s: %s\n", str, strerror(errno))


void fail(const char *msg= "")
{
    flog(LOG_CRIT, "%s: %s\n", msg, strerror(errno));
    exit(1);
}



// set an fd to non-blocking mode
inline bool setNonblocking(int fd, bool on= true)
{
	int opts= fcntl(fd, F_GETFL);
	if(opts<0)
    {
		logerror("fcntl(F_GETFL)");
		return false;
	}
	if(on) opts|= O_NONBLOCK;
	else opts&= (~O_NONBLOCK);
	if(fcntl(fd, F_SETFL, opts)<0)
	{
		logerror("fcntl(F_SETFL)");
		return false;
	}
	return true;
}


// base class for handling buffered writes to a non-blocking fd.
class NonblockWriter
{
    public:
        NonblockWriter(): fd(-1) {}

        void setFd(int _fd) { fd= _fd; setNonblocking(fd); }
		int getFd() { return fd; }

        // try flushing the write buffer.
        bool flush()
        {
            while(!buffer.empty())
            {
                size_t sz= writeToFile(&buffer.front()[0], buffer.front().size());
                if(sz==buffer.front().size())
                    buffer.pop_front();
                else
                {
                    buffer.front().erase(buffer.front().begin(), buffer.front().begin()+sz);
                    return false;
                }
            }
            return true;
        }

        bool writeBufferEmpty()
        { return buffer.empty(); }
		
		void write(vector<char> data)
		{
            buffer.push_back(data);
            flush();
		}
		
		void write(const char *data, size_t len)
		{
			write( vector<char>(data+0, data+len) );
		}

        // write or buffer a string.
        void writeString(const string s)
        {
			write(s.data(), s.size());
        }

        // write or buffer a printf-style string.
        void writef(const char *fmt, ...)
        {
            char c[2048];
            va_list ap;
            va_start(ap, fmt);
            vsnprintf(c, sizeof(c), fmt, ap);
            va_end(ap);
            writeString(c);
        }

        // calculate the size of the write buffer in bytes.
        size_t getWritebufferSize()
        {
            size_t ret= 0;
            for(deque< vector<char> >::iterator it= buffer.begin(); it!=buffer.end(); it++)
                ret+= it->size();
            return ret;
        }

        // error callback.
        virtual void writeFailed(int _errno)= 0;

    private:
        int fd;
        deque< vector<char> > buffer;

        // write a piece of data without buffering. return number of bytes written.
        size_t writeToFile(const char *data, size_t size)
        {
            ssize_t sz= ::write(fd, data, size);
            if(sz<0)
            {
                if( (errno!=EAGAIN)&&(errno!=EWOULDBLOCK) )
                    logerror("write"),
                    writeFailed(errno);
                return 0;
            }
            return sz;
        }
};


#endif //UTILS_H
