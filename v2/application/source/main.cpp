#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

#include <iostream>
#include <cstring>

using std::cout;
using std::endl;
using std::string;

extern string getNowTime();

int main(int argc, char *argv[])
{
	int sockfd, connfd;
	sockaddr_in serverAddr;
	memset(&serverAddr, 0, sizeof(sockaddr_in));

	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(8888);
	serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);

	sockfd = socket(AF_INET, SOCK_STREAM, 0);

	bind(sockfd, reinterpret_cast<const sockaddr *>(&serverAddr), sizeof(sockaddr_in));

	listen(sockfd, 10);

	while (true)
	{
		connfd = accept(sockfd, nullptr, nullptr);

		char request[1501];

		recv(connfd, request, 1500, 0);

		cout << getNowTime() << "receive a message" << endl;

		cout << request;

		memset(request, 0, sizeof(request));

		close(connfd);
	}

	close(sockfd);
}