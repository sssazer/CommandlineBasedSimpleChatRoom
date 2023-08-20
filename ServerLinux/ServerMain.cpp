#include<iostream>
#include<vector>
#include<sys/types.h>
#include<sys/socket.h>
#include<unistd.h>
#include <netdb.h>
#include <arpa/inet.h>
#include<errno.h>
#include<netinet/in.h>
#include<sys/epoll.h>
#include<pthread.h>
#include<string.h>
#include<unordered_map>
#include<iconv.h>
#include<unordered_set>

using namespace std;

int epfd = epoll_create(40);
struct epoll_event events[20];
int listen_socket;
unordered_map<int, string> user_list; // key = socket, value = user_name
unordered_set<int> broken_socket; // 保存异常断开连接的客户端socket

int createSocket(int port) {
	/*********创建socket***************/
	listen_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (listen_socket == -1) {
		cout << "Can't create a socket" << endl;
		shutdown(listen_socket, SHUT_RDWR);
		return -1;
	}
	else {
		cout << "成功创建Socket" << endl;
	}

	/*************绑定IP端口号********************/
	sockaddr_in server_addr;
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = htonl(INADDR_ANY); // 绑定本地能绑定的所有IP地址
	server_addr.sin_port = htons(port);
	if (bind(listen_socket, (sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
		cout << "绑定失败:" << errno << endl;
		shutdown(listen_socket, SHUT_RDWR);
		return -2;
	}
	else {
		cout << "绑定成功" << endl;
	}

	/*************Listening**********************/
	if (listen(listen_socket, SOMAXCONN) == -1) {
		cout << "无法监听,错误代码:" << errno << endl;
		shutdown(listen_socket, SHUT_RDWR);
		return -3;
	}
	else {
		struct epoll_event ev;
		ev.data.fd = listen_socket;
		ev.events = EPOLLIN;
		epoll_ctl(epfd, EPOLL_CTL_ADD, ev.data.fd, &ev);
		cout << "正在监听..." << endl;
	}

	return 0;
}


int UTF2GB(char* szSrc, size_t iSrcLen, char* szDst, size_t iDstLen) {
	iconv_t cd = iconv_open("gb2312//IGNORE", "utf8//IGNORE");
	if (0 == cd) {
		return -2;
	}
	memset(szDst, 0, iDstLen);
	char** src = &szSrc;
	char** dst = &szDst;
	if (-1 == (int)iconv(cd, src, &iSrcLen, dst, &iDstLen))
		return -1;
	iconv_close(cd);
	return 0;
}

int GB2UTF(char* szSrc, size_t iSrcLen, char* szDst, size_t iDstLen) {
	iconv_t cd = iconv_open("utf-8//IGNORE", "gb2312//IGNORE");
	if (0 == cd)
		return -2;
	memset(szDst, 0, iDstLen);
	char** src = &szSrc;
	char** dst = &szDst;
	if (-1 == (int)iconv(cd, src, &iSrcLen, dst, &iDstLen))
		return -1;
	iconv_close(cd);
	return 0;
}

int processData(){

	while (true) {
		/**********等待消息********/
		int nfds = epoll_wait(epfd, events, SOMAXCONN, -1);
		/******** 处理消息 ******/
		for (int i = 0; i < nfds; i++) {
			int cur_socket = events[i].data.fd;
			/*****************处理客户端连接请求**************************/
			if (cur_socket == listen_socket) {
				sockaddr_in clientAddr;
				socklen_t clientSize = sizeof(clientAddr);
				//char host[NI_MAXHOST];
				//char svc[NI_MAXSERV];

				int acceptSocket = accept(listen_socket, (sockaddr*)&clientAddr, &clientSize);
				if (acceptSocket == -1) {
					cout << "连接失败,错误代码" << errno << endl;
					shutdown(listen_socket, SHUT_RDWR);
					return -4;
				}
				else {
					char *IP_str = inet_ntoa(clientAddr.sin_addr);
					cout << "与" << IP_str << ":" << clientAddr.sin_port << "连接成功" << endl;
					static struct epoll_event ev;
					ev.data.fd = acceptSocket;
					ev.events = EPOLLIN;
					epoll_ctl(epfd, EPOLL_CTL_ADD, ev.data.fd, &ev);
				}			
			}
			//else if (broken_socket.find(cur_socket) != broken_socket.end()) { // 如果客户端异常退出，就不处理这个消息
			//	epoll_ctl(epfd, EPOLL_CTL_DEL, cur_socket, 0);
			//	user_list.erase(user_list.find(cur_socket));
			//	close(cur_socket);
			//	broken_socket.erase(broken_socket.find(cur_socket));
			//}
			/*****************处理客户端发来的数据*****************/
			else {
				// 读取数据
				char buffer[200] = "";
				int recv_size = read(cur_socket, buffer, sizeof(buffer));
				if (recv_size <= 0 || buffer[0] == '\0') { // 接收到的数据大小为0表示用户断开连接
					string exit_name = user_list[cur_socket];
					cout << exit_name << "断开连接" << endl;
					// 断开连接，清除socket对象
					epoll_ctl(epfd, EPOLL_CTL_DEL, cur_socket, 0);
					user_list.erase(user_list.find(cur_socket));
					close(cur_socket);
					// 给聊天室其他人播报退出信息
					for (pair<const int, string>& p : user_list) {
						string msg = "-------【" + exit_name + "】 : 退出聊天------";
						char msg_gb[200] = "";
						UTF2GB((char*)msg.c_str(), 200, msg_gb, 200);
						cout << "msg_gb" << msg_gb << endl;
						send(p.first, &msg_gb, 200, 0);
					}
				}
				else { // 收到了客户端发来的消息
					// windows发来的数据是gbk格式，需要转为utf-8
					char utf_buffer[200] = "";
					GB2UTF(buffer, sizeof(buffer), utf_buffer, sizeof(utf_buffer));
					cout << utf_buffer << endl;
					// 将接收到的消息转发给其他用户
					if (user_list.find(cur_socket) == user_list.end()) { // 如果是刚注册的新用户发送的用户名
						user_list[cur_socket] = utf_buffer;
						cout << utf_buffer << "注册成功" << endl;
						for (pair<const int, string>& p : user_list) {
							string msg = "-------【" + user_list[cur_socket] + "】 : 加入聊天------";
							char msg_gb[200] = "";
							UTF2GB((char*)msg.c_str(), 200, msg_gb, 200);
							cout << "msg_gb" << msg_gb << endl;
							send(p.first, &msg_gb, 200, 0);
						}
					}
					else { // 已注册用户发送的聊天信息
						// 生成要发送的信息，并转为gbk格式
						string msg = "【" + user_list[cur_socket] + "】" + " : " + utf_buffer;
						char msg_gb[200] = "";
						UTF2GB((char*)msg.c_str(), 200, msg_gb, 200);
						cout << "msg_gb: " << msg_gb << endl;

						// 给所有用户发送消息
						for (pair<const int, string>& p : user_list) {
							if (p.first == cur_socket) {
								string self_msg = "【" + p.second + "（我）】: " + utf_buffer;
								char self_msg_gb[200] = "";
								UTF2GB((char*)self_msg.c_str(), 200, self_msg_gb, 200);
								send(p.first, &self_msg_gb, 200, MSG_NOSIGNAL);
							}
							else {
								send(p.first, &msg_gb, 200, MSG_NOSIGNAL);
							}
						}
					}
				} // 收到了客户端的消息
			} // 处理客户端发来的消息
			
		} // for
	}
}

int main() {

	int status = createSocket(55555);
	if (status < 0) {
		return 0;
	}
	processData();

	shutdown(listen_socket, SHUT_RDWR);
	close(listen_socket);
	return 0;
}