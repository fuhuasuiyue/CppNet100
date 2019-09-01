#ifndef _EASYTCPSERVER_HPP_
#define _EASYTCPSERVER_HPP_

#ifdef _WIN32
	#define WIN32_LEAN_AND_MEAN
	#include <Windows.h>
	#include <WinSock2.h>

	#pragma comment(lib, "ws2_32.lib")
#else
	#include <unistd.h>	    // uni std
	#include <arpa/inet.h>
	#include <sys/types.h>
	#include <sys/socket.h>
	#include <string.h>

	#define SOCKET int
	#define INVALID_SOCKET  (SOCKET)(~0)
	#define SOCKET_ERROR            (-1)
#endif

#include "MessageHeader.hpp"

class EasyTcpServer
{
public:
	EasyTcpServer()
	{
		m_sock = INVALID_SOCKET;
	}

	virtual ~EasyTcpServer()
	{
		Close();
	}

	// ��ʼ��socket
	int InitSock()
	{
		// ����Windows Socket 2.x����
#ifdef _WIN32
//----------------------
// Initialize Winsock
		WSADATA wsaData;
		int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
		if (iResult != NO_ERROR)
		{
			printf("WSAStartup() ���󣬴����׽��ֿ�ʧ��!\n");
			return -1;
		}
#endif

		// ����һ��Sokcet
		if (m_sock != INVALID_SOCKET)
		{
			printf("<Socket=%d>�رվɵ�����...\n", (int)m_sock);
			Close();
		}

		m_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (m_sock == INVALID_SOCKET) {
			printf("�����׽���ʧ��!\n");
			return -1;
		}
		else
		{
			printf("�����׽���<socket=%d>�ɹ�...\n", (int)m_sock);

			return 0;
		}
	}

	// ��IP�Ͷ˿ں�
	int Bind(const char* ip, unsigned short port)
	{
		/*if (INVALID_SOCKET == m_sock)
		{
			InitSock();
		}*/

		sockaddr_in service;
		service.sin_family = AF_INET;
		service.sin_port = htons(port);
#ifdef _WIN32
		if (ip)
		{
			service.sin_addr.S_un.S_addr = inet_addr(ip);
		}
		else
		{
			service.sin_addr.S_un.S_addr = INADDR_ANY;
		}
#else
		if (ip)
		{
			service.sin_addr.s_addr = inet_addr(ip);
		}
		else
		{
			service.sin_addr.s_addr = INADDR_ANY;
		}
#endif
		int ret = bind(m_sock, (sockaddr*)&service, sizeof(service));
		if (SOCKET_ERROR == ret) 
		{
			printf("bind() failed.������˿ں�<%d>ʧ��\n", port);
		}
		else
		{
			printf("������˿�<%d>�ɹ�...\n", port);
		}

		return ret;
	}

	// �����˿ں�
	int Listen(int maxConnNum)
	{
		int ret = listen(m_sock, maxConnNum);
		if (SOCKET_ERROR == ret) 
		{
			printf("socket=<%d>���󣬼�������˿�ʧ��...\n", (int)m_sock);
		}
		else
		{
			printf("socket=<%d>��������˿ڳɹ�...\n", (int)m_sock);
		}

		return ret;
	}

	// ���տͻ�������
	SOCKET Accept()
	{
		// 4. accept �ȴ����ܿͻ�������
		sockaddr_in clientAddr = {};
		int nAddrLen = sizeof(sockaddr_in);
		SOCKET ClientSocket = INVALID_SOCKET;
#ifndef _WIN32
		ClientSocket = accept(m_sock, (sockaddr*)&clientAddr, (socklen_t*)&nAddrLen);
#else
		ClientSocket = accept(m_sock, (sockaddr*)&clientAddr, &nAddrLen);
#endif
		if (INVALID_SOCKET == ClientSocket)
		{
			printf("socket=<%d>����,���յ���Ч�ͻ���Socket...\n", (int)m_sock);
		}
		else
		{
			// ���µĿͻ��˼��룬��֮ǰ�����пͻ���Ⱥ����Ϣ
			NewUserJoin userJoin;
			SendDataToAll(&userJoin);

			m_clientVec.push_back(ClientSocket);
			// �ͻ������ӳɹ�������ʾ�ͻ������ӵ�IP��ַ�Ͷ˿ں�
			printf("socket=<%d>�¿ͻ���<sokcet=%d>����,Ip��ַ��%s,�˿ںţ�%d\n",
				(int)m_sock, (int)ClientSocket, inet_ntoa(clientAddr.sin_addr),
				ntohs(clientAddr.sin_port));
		}

		return ClientSocket;
	}
	// �ر�Sokcet
	void Close()
	{
		if (INVALID_SOCKET != m_sock)
		{
#ifdef _WIN32
			for (int n = (int)m_clientVec.size() - 1; n >= 0; n--)
			{
				closesocket(m_clientVec[n]);
			}
			closesocket(m_sock);
			// ���Windows Socket����
			WSACleanup();
#else
			for (int n = (int)m_clientVec.size() - 1; n >= 0; n--)
			{
				close(m_clientVec[n]);
			}
			close(m_sock);
#endif
			m_sock = INVALID_SOCKET;
		}
	}
	
	// ����������Ϣ
	bool OnRun()
	{
		if (IsRun())
		{
			// Berkeley sockets
			fd_set readfds;			// ������(socket)����
			fd_set writefds;
			fd_set exceptfds;

			// ��������
			FD_ZERO(&readfds);
			FD_ZERO(&writefds);
			FD_ZERO(&exceptfds);

			// ��������(socket)���뼯��
			FD_SET(m_sock, &readfds);
			FD_SET(m_sock, &writefds);
			FD_SET(m_sock, &exceptfds);

			// ��ȡ�׽������������
			SOCKET maxSock = m_sock;

			for (int n = (int)m_clientVec.size() - 1; n >= 0; n--)
			{
				FD_SET(m_clientVec[n], &readfds);

				if (m_clientVec[n] > maxSock)
				{
					maxSock = m_clientVec[n];
				}
			}

			// ���ó�ʱʱ�� select ������
			timeval timeout = { 1, 0 };

			// nfds��һ������ֵ����ָfd_set����������������(socket)�ķ�Χ������������
			// ���������ļ����������ֵ+1 ��Windows�������������д0
			//int ret = select(ListenSocket + 1, &readfds, &writefds, &exceptfds, NULL);
			int ret = select(maxSock + 1, &readfds, &writefds, &exceptfds, &timeout);
			if (ret < 0)
			{
				printf("select�������...\n");
				Close();
				return false;
			}

			// �Ƿ������ݿɶ�
			// �ж�������(socket)�Ƿ��ڼ�����
			if (FD_ISSET(m_sock, &readfds))
			{
				FD_CLR(m_sock, &readfds);

				Accept();
			}

			for (int n = (int)m_clientVec.size() - 1; n >= 0; n--)
			{
				if (FD_ISSET(m_clientVec[n], &readfds))
				{
					if (-1 == RecvData(m_clientVec[n]))
					{

						auto iter = m_clientVec.begin() + n;
						if (iter != m_clientVec.end())
						{
							m_clientVec.erase(iter);
						}
					}
				}
			}

			return true;
		}

		return false;
	}

	// �Ƿ�����
	bool IsRun()
	{
		return m_sock != INVALID_SOCKET;
	}

	// �������� ����ճ�� ��ְ�
	int RecvData(SOCKET sock)
	{
		// ������(4096�ֽ�)
		char szRecv[4096] = {};
		// 5�����տͻ��˵�����
		// �Ƚ�����Ϣͷ
		int recvLen = (int)recv(sock, szRecv, sizeof(DataHeader), 0);
		DataHeader *pHeader = (DataHeader*)szRecv;
		if (recvLen <= 0)
		{
			printf("�ͻ���<Socket=%d>���˳����������...\n", sock);
			return -1;
		}

		recv(sock, szRecv + sizeof(DataHeader), pHeader->dataLength - sizeof(DataHeader), 0);

		// ��Ӧ������Ϣ
		OnNetMsg(sock, pHeader);

		return 0;
	}

	// ��Ӧ������Ϣ
	virtual void OnNetMsg(SOCKET clientSock, DataHeader* pHeader)
	{
		// 6����������
		switch (pHeader->cmd)
		{
		case CMD_LOGIN:
		{
			Login *login = (Login*)pHeader;

			printf("�յ��ͻ���<Socket=%d>����CMD_LOGIN, ���ݳ��ȣ�%d, userName��%s Password�� %s\n",
				m_sock, login->dataLength, login->userName, login->passWord);
			// �����ж��û����������Ƿ���ȷ�Ĺ���
			LoginResult ret;
			send(clientSock, (char*)&ret, sizeof(LoginResult), 0);
		}
		break;
		case CMD_LOGOUT:
		{
			Logout *logout = (Logout*)pHeader;
			
			printf("�յ��ͻ���<Socket=%d>����CMD_LOGOUT, ���ݳ��ȣ�%d, userName��%s\n",
				m_sock, logout->dataLength, logout->userName);
			LogoutResult ret;
			send(clientSock, (char*)&ret, sizeof(LogoutResult), 0);
		}
		break;
		default:
		{
			DataHeader header = { 0, CMD_ERROR };
			send(clientSock, (char*)&header, sizeof(header), 0);
			break;
		}
		}
	}

	// ��������(����) ���͸�ָ���Ŀͻ���Socket
	int SendData(SOCKET sock, DataHeader* pHeader)
	{
		if (IsRun() && pHeader)
		{
			return send(sock, (const char*)pHeader, pHeader->dataLength, 0);
		}

		return SOCKET_ERROR;
	}

	// ��������(Ⱥ��)
	void SendDataToAll(DataHeader* pHeader)
	{
		for (int n = (int)m_clientVec.size() - 1; n >= 0; n--)
		{
			SendData(m_clientVec[n], pHeader);
		}
	}

private:
	SOCKET m_sock;
	std::vector<SOCKET>	m_clientVec;	// �ͻ����׽����б�
};

#endif	// ! _EASYTCPSERVER_HPP_