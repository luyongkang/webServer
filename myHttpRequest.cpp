#include "myHttpRequest.h"
#include "myTimer.h"
#include "myUtil.h"
#include "myEpoll.h"

#include <sys/mman.h>
#include <sys/stat.h>
#include <iostream>
#include <unistd.h>
#include <sstream>
#include <string>
#include <cstring>
#include <sys/socket.h>
#include <fcntl.h>
using std::cerr;
using std::endl;
using std::string;
using std::unordered_map;

const int STATE_PARSE_URI = 1;
const int STATE_PARSE_HEADERS = 2;
const int STATE_RECV_BODY = 3;
const int STATE_ANALYSIS = 4;
const int STATE_FINISH = 5;
const int MAX_BUFFER = 4096;
const int AGAIN_TIMES_MAX = 200;

const int PARSE_URI_AGAIN = -1;
const int PARSE_URI_ERROR = -2;
const int PARSE_URI_SUCCESS = 0;

const int PARSE_HEADER_AGAIN = -1;
const int PARSE_HEADER_ERROR = -2;
const int PARSE_HEADER_SUCCESS = 0;

const int ANALYSIS_ERROR = -2;
const int ANALYSIS_SUCCESS = 0;

const int METHOD_POST = 1;
const int METHOD_GET = 2;
const int HTTP_10 = 1;
const int HTTP_11 = 2;

const int EPOLL_WAIT_TIME = 500;

unordered_map<string, string> mimeType::mime;

pthread_mutex_t mimeType::lock = PTHREAD_MUTEX_INITIALIZER;

string mimeType::getMime(const string &suffix)
{
	if (mime.size() == 0)
	{
		pthread_mutex_lock(&lock);
		mime[".html"] = "text/html";
		mime[".htm"] = "text/html";
		mime[".css"] = "text/css";
		mime[".js"] = "text/javascript";
		mime[".png"] = "image/png";
		mime[".jpg"] = "image/jpeg";
		mime[".jpeg"] = "image/jpeg";
		mime[".json"] = "application/json";
		mime[".txt"] = "text/plain";
		mime[".mp4"] = "video/mp4";
		mime[".avi"] = "video/x-msvideo";
		mime[".bmp"] = "image/bmp";
		mime[".c"] = "text/plain";
		mime[".doc"] = "application/msword";
		mime[".gif"] = "image/gif";
		mime[".gz"] = "application/x-gzip";
		mime[".ico"] = "application/x-ico";
		mime[".jpg"] = "image/jpeg";
		mime[".txt"] = "text/plain";
		mime[".mp3"] = "audio/mp3";
		mime["default"] = "text/html";

		pthread_mutex_unlock(&lock);
	}

	if (mime.find(suffix) == mime.end())
	{
		return mime["default"];
	}
	else
	{
		return mime.at(suffix);
	}
}

httpRequest::httpRequest() : now_read_pos(0), state(STATE_PARSE_URI), h_state(h_start),
							 keep_alive(false), againTimes(0), timer(nullptr) {}

httpRequest::httpRequest(int _epfd, int _fd, string _path) : now_read_pos(0), state(STATE_PARSE_URI), h_state(h_start),
															 keep_alive(false), againTimes(0), timer(NULL),
															 path(_path), fd(_fd), epollfd(_epfd) {}

httpRequest::~httpRequest()
{
	//先从epoll监听列表中删除fd
	myEpollDel(epollfd, fd);
	//然后将与httpRequest关联的mytimer，消除关联
	if (timer != nullptr)
	{
		timer->clearReq();
		timer = nullptr;
	}
	close(fd);
}

void httpRequest::addTimer(mytimer *ptime)
{
	if (timer == nullptr)
	{
		timer = ptime;
	}
}

void httpRequest::seperateTimer()
{
	if (timer != nullptr)
	{
		timer->clearReq();
		timer = nullptr;
	}
}

void httpRequest::setFd(int _fd)
{
	fd = _fd;
}

int httpRequest::getFd() const
{
	return fd;
}

void httpRequest::reset()
{
	againTimes = 0;
	content.clear();
	fileName.clear();
	path.clear();
	now_read_pos = 0;
	state = STATE_PARSE_URI;
	h_state = h_start;
	headers.clear();
	keep_alive = false;
}

void httpRequest::handleRequest()
{
	char buf[MAX_BUFFER];
	bool isError = false;
	while (true)
	{
		int readNum = readn(fd, buf, MAX_BUFFER);
		if (readNum == -1)
		{
			cerr << getNowTime() << ": httpRequest::handleRequest() error" << endl;
			isError = true;
			break;
		}
		else if (readNum == 0)
		{
			//有请求但是没有读到数据，可能是因为Request Aborted，也可能是网络数据还没有到达
			cerr << getNowTime() << ": httpRequest::handleRequest() error" << endl;
			if (errno == EAGAIN)
			{
				if (againTimes > AGAIN_TIMES_MAX)
				{
					isError = true;
					break;
				}
				else
				{
					againTimes++;
					continue;
				}
			}
			else if (errno != 0)
			{
				isError = true;
				break;
			}
		}

		//readNum>0
		content = content + string(buf, buf + readNum);

		if (state == STATE_PARSE_URI)
		{
			int flag = parseURI();
			if (flag == PARSE_URI_AGAIN || flag == PARSE_URI_ERROR)
			{
				isError = true;
				break;
			}
		}

		if (state == STATE_PARSE_HEADERS)
		{
			int flag = parseHeader();
			if (flag == PARSE_HEADER_AGAIN || flag == PARSE_HEADER_ERROR)
			{
				isError = true;
				break;
			}
		}

		if (state == STATE_RECV_BODY)
		{
			int content_length = -1;
			if (headers.find("Content-length") != headers.end())
			{
				content_length = stoi(headers["Content-length"]);
			}
			else
			{
				isError = true;
				break;
			}
			if (content.size() < content_length)
				continue;
			state = STATE_ANALYSIS;
		}

		if (state == STATE_ANALYSIS)
		{
			int flag = analysisRequest();
			if (flag < 0)
			{
				isError = true;
				break;
			}
			else if (flag == ANALYSIS_SUCCESS)
			{
				state = STATE_FINISH;
				break;
			}
			else
			{
				isError = true;
				break;
			}
		}
	}

	if (isError == true)
	{
		delete this;
		return;
	}
	if (state == STATE_FINISH)
	{
		if (keep_alive)
		{
			reset();
		}
		else
		{
			delete this;
			return;
		}
	}

	//如果要keep_alive，那么就需要把该fd重新放入epoll监听
	pthread_mutex_lock(&myTimerLock);
	mytimer *ptime = new mytimer(this, TIME_TIMER_OUT);
	timer = ptime;
	myTimerQueue.push(timer);
	pthread_mutex_unlock(&myTimerLock);
	epoll_event event;
	event.events = EPOLLIN | EPOLLET | EPOLLONESHOT;
	event.data.ptr = this;
	myEpollAdd(epollfd, fd, &event);
}

int httpRequest::parseURI()
{
	//首先将第一行（即请求行取出）
	int pos = content.find('\n', now_read_pos);
	if (pos == content.npos)
	{
		return PARSE_URI_AGAIN;
	}
	string requestLine = content.substr(0, pos); //取出第一行，不包括/r/n
	if (content.size() > pos + 2)
	{
		content = content.substr(pos + 2);
	}
	else
	{
		content.clear();
	}

	//判断是哪个method
	pos = requestLine.find("GET");
	if (pos != 0)
	{
		pos = requestLine.find("POST");
		if (pos != 0)
		{
			return PARSE_URI_ERROR;
		}
		else
		{
			method = METHOD_POST;
		}
	}
	else
	{
		method = METHOD_GET;
	}

	//找到请求的URL
	pos = requestLine.find('/');
	if (pos == requestLine.npos)
	{
		return PARSE_URI_ERROR;
	}
	else
	{
		int endPos = requestLine.find(' ', pos);
		if (endPos == requestLine.npos)
		{
			return PARSE_URI_ERROR;
		}
		if (endPos - pos > 1)
		{
			int _endPos = requestLine.find('?', pos);
			if (_endPos == requestLine.npos)
			{
				_endPos = endPos;
			}
			fileName = requestLine.substr(pos + 1, _endPos - pos - 1);
		}
		else
		{
			fileName = "index.html";
		}
		pos = endPos;
	}

	//找到请求行的http版本号
	pos = requestLine.find('/', pos);
	if (pos == requestLine.npos)
	{
		return PARSE_URI_ERROR;
	}
	else
	{
		if (requestLine.size() - pos <= 3)
		{
			return PARSE_URI_ERROR;
		}
		else
		{
			string tmpStr = requestLine.substr(pos + 1, 3);
			if (tmpStr == "1.0")
			{
				httpVersion = HTTP_10;
			}
			else if (tmpStr == "1.1")
			{
				httpVersion = HTTP_11;
			}
			else
			{
				return PARSE_URI_ERROR;
			}
		}
	}

	state = STATE_PARSE_HEADERS;
	return PARSE_URI_SUCCESS;
}

int httpRequest::parseHeader()
{
	int keyBegin, keyEnd, valueBegin, valueEnd;
	bool notFinish = true;
	int index = 0;
	for (index = 0; index < content.size() && notFinish; index++)
	{
		switch (h_state)
		{
		case h_start:
		{
			if (content[index] == '\r' || content[index] == '\n')
			{
				break;
			}
			h_state = h_key;
			keyBegin = index;
			break;
		}
		case h_key:
		{
			if (content[index] == ':')
			{
				keyEnd = index;
				if (keyEnd - keyBegin <= 0)
				{
					return PARSE_HEADER_ERROR;
				}
				h_state = h_colon;
			}
			else if (content[index] == '\r')
			{
				return PARSE_HEADER_ERROR;
			}
			else
				break;
		}
		case h_colon:
		{
			if (content[index] == ' ')
			{
				h_state = h_spaces_after_colon;
			}
			else
			{
				return PARSE_HEADER_ERROR;
			}
			break;
		}
		case h_spaces_after_colon:
		{
			h_state = h_value;
			valueBegin = index;
			break;
		}
		case h_value:
		{
			if (content[index] == '\r')
			{
				h_state = h_CR;
				valueEnd = index;
				if (valueEnd - valueBegin <= 0)
				{
					return PARSE_HEADER_ERROR;
				}
			}
			else if (index - valueBegin > 255)
			{
				return PARSE_HEADER_ERROR;
			}
			break;
		}
		case h_CR:
		{
			if (content[index] == '\n')
			{
				h_state = h_LF;
				string key(content.begin() + keyBegin, content.begin() + keyEnd);
				string value(content.begin() + valueBegin, content.begin() + valueEnd);
				headers[key] = value;
			}
			else
				return PARSE_HEADER_ERROR;
			break;
		}
		case h_LF:
		{
			if (content[index] == '\r')
			{
				h_state = h_end_CR;
			}
			else
			{
				keyBegin = index;
				h_state = h_key;
			}
			break;
		}
		case h_end_CR:
		{
			if (content[index] == '\n')
			{
				h_state = h_end_LF;
			}
			else
				return PARSE_HEADER_ERROR;
			break;
		}
		case h_end_LF:
		{
			notFinish = false;
			keyBegin = index;
			break;
		}
		}
	}
	if (h_state == h_end_LF)
	{
		content = content.substr(index);
		return PARSE_HEADER_SUCCESS;
	}
	content = content.substr(index);
	return PARSE_HEADER_AGAIN;
}

int httpRequest::analysisRequest()
{
	if (method == METHOD_POST)
	{
		char header[MAX_BUFFER];
		sprintf(header, "HTTP/1.1 %d %s\r\n", 200, "OK");
		if (headers.find("Connection") != headers.end() && headers.at("Connection") == "keep-alive")
		{
			keep_alive = true;
			sprintf(header, "%sConnection: keep-alive\r\n", header);
			sprintf(header, "%sKeep-Alive: timeout=%d\r\n", header, EPOLL_WAIT_TIME);
		}

		const char *sendMessage = "Server receiced a post request";
		sprintf(header, "%sContent-length: %ld\r\n", header, strlen(sendMessage));
		sprintf(header, "%s\r\n", header);
		sprintf(header, "%s%s\r\n", header, sendMessage);
		int sentNum = send(fd, header, strlen(header), 0);
		if (sentNum == -1)
		{
			return ANALYSIS_ERROR;
		}
		return ANALYSIS_ERROR;
	}
	else if (method == METHOD_GET)
	{
		char header[MAX_BUFFER];
		sprintf(header, "HTTP/1.1 %d %s\r\n", 200, "OK");
		if (headers.find("Connection") != headers.end() && headers.at("Connection") == "keep-alive")
		{
			keep_alive = true;
			sprintf(header, "%sConnection: keep-alive\r\n", header);
			sprintf(header, "%sKeep-Alive: timeout=%d\r\n", header, EPOLL_WAIT_TIME);
		}

		int dotPos = fileName.find('.');
		const char *filetype;
		if (dotPos == fileName.npos)
		{
			filetype = mimeType::getMime("default").c_str();
		}
		else
		{
			filetype = mimeType::getMime(fileName.substr(dotPos + 1)).c_str();
		}

		struct stat sbuf;
		if (stat(fileName.c_str(), &sbuf) < 0)
		{
			handleError(fd, 404, "not found");
			return ANALYSIS_ERROR;
		}
		sprintf(header, "%sContent-type: %s\r\n", header, filetype);
		// 通过Content-length返回文件大小
		sprintf(header, "%sContent-length: %ld\r\n", header, sbuf.st_size);

		sprintf(header, "%s\r\n", header);
		size_t send_len = (size_t)writen(fd, header, strlen(header));
		if (send_len != strlen(header))
		{
			return ANALYSIS_ERROR;
		}
		int src_fd = open(fileName.c_str(), O_RDONLY, 0);
		char *src_addr = static_cast<char *>(mmap(NULL, sbuf.st_size, PROT_READ, MAP_PRIVATE, src_fd, 0));
		close(src_fd);

		// 发送文件并校验完整性
		send_len = writen(fd, src_addr, sbuf.st_size);
		if (send_len != sbuf.st_size)
		{
			perror("Send file failed");
			return ANALYSIS_ERROR;
		}
		munmap(src_addr, sbuf.st_size);
		return ANALYSIS_SUCCESS;
	}
	else
	{
		return ANALYSIS_ERROR;
	}
}

void httpRequest::handleError(int fd, int err_num, string short_msg)
{
	short_msg = " " + short_msg;
	char send_buff[MAX_BUFFER];
	string body_buff, header_buff;
	body_buff += "<html><title>Error</title>";
	body_buff += "<body bgcolor=\"ffffff\">";
	body_buff += std::to_string(err_num) + short_msg;
	body_buff += "<hr><em>Web Server</em>\n</body></html>";

	header_buff += "HTTP/1.1 " + std::to_string(err_num) + short_msg + "\r\n";
	header_buff += "Content-type: text/html\r\n";
	header_buff += "Connection: close\r\n";
	header_buff += "Content-length: " + std::to_string(body_buff.size()) + "\r\n";
	header_buff += "\r\n";
	sprintf(send_buff, "%s", header_buff.c_str());
	writen(fd, send_buff, strlen(send_buff));
	sprintf(send_buff, "%s", body_buff.c_str());
	writen(fd, send_buff, strlen(send_buff));
}