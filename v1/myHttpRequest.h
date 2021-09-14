#ifndef MYHTTPREQUEST_H
#define MYHTTPREQUEST_H

#include <unordered_map>
#include <string>


class mimeType
{
public:
	static std::string getMime(const std::string &suffix);

private:
	static std::unordered_map<std::string, std::string> mime;
	static pthread_mutex_t lock;

	mimeType();
	mimeType(const mimeType &arg);
};

enum HeadersState
{
	h_start = 0,
	h_key,
	h_colon,
	h_spaces_after_colon,
	h_value,
	h_CR,
	h_LF,
	h_end_CR,
	h_end_LF
};

class mytimer;//当有两个类互相包含对方的对象时，可以用指针代替对象，并且在类定义前声明对方类，如果这两个类还在不同文件，那头文件都不要包含而是在源文件中去包含头文件

class httpRequest
{
private:
	int againTimes;
	std::string path;
	int fd;
	int epollfd;
	std::string content;//http报文内容
	int method;
	int httpVersion;
	std::string fileName;
	int now_read_pos;
    int state;//当前所处的http报文解析的阶段
    int h_state;
    bool isfinish;
    bool keep_alive;//http1.0与http1.1的非持久iu化连接，持久化连接
    std::unordered_map<std::string, std::string> headers;
    mytimer *timer;

	int parseURI();
	int parseHeader();
	int analysisRequest();

public:
	httpRequest();
	httpRequest(int _epfd, int _fd, std::string _path);
	~httpRequest();
	void addTimer(mytimer *ptimer);
	void reset();
	void seperateTimer();
	int getFd() const;
	void setFd(int _fd);
	void handleRequest();
	void handleError(int _fd, int errNum, std::string msg);
};

#endif