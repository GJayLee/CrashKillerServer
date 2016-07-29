#pragma once
#include<iostream>
#include<boost\asio.hpp>

#include "Message.h"

#include "MyHttpHandler.h"

using namespace boost::asio;
using boost::asio::ip::tcp;

const int MAX_IP_PACK_SIZE = 100;
const int HEAD_LEN = 1;

const int SEND_SIZE = 100;

class RWHandler
{
public:

	RWHandler(io_service& ios) : m_sock(ios)
	{
		offSet = 0;
	}

	~RWHandler()
	{
	}

	void HandleRead()
	{
		//��������»᷵�أ�1.����������2.transfer_at_leastΪ��(�յ��ض������ֽڼ�����)��3.�д�����
		//async_read(m_sock, buffer(m_buff), transfer_at_least(HEAD_LEN), [this](const boost::system::error_code& ec, size_t size)
		//{
		//	if (ec != nullptr)
		//	{
		//		HandleError(ec);
		//		return;
		//	}
		//	
		//	//std::cout << m_buff.size() << "," << m_buff.data() << std::endl;

		//	//HandleRead();
		//	offSet = offSet + 10;
		//	HandleWrite();
		//});

		try 
		{
			//������Ϣ����������
			boost::shared_ptr<std::vector<char> > str(new std::vector<char>(100, 0));
			m_sock.async_read_some(buffer(*str), bind(&RWHandler::read_handler, this, placeholders::error, str));
		}
		catch(std::exception& e)
		{
			std::cout << "��ȡ����" << std::endl;
		}
	}

	//void HandleWrite(char* data, int len)
	void HandleWrite()
	{
		/*boost::system::error_code ec;
		write(m_sock, buffer(data, len), ec);
		if (ec != nullptr)
			HandleError(ec);*/
		// ������Ϣ(������)
		/*boost::shared_ptr<std::string> pstr(new
			std::string(errorList));
		m_sock.async_write_some(buffer(str, len),
			boost::bind(&RWHandler::write_handler, this, placeholders::error),pstr);*/
		int sendSize = 0;
		if (strlen(sendData) - offSet < SEND_SIZE)
			sendSize = strlen(sendData) - offSet;
		else
			sendSize = SEND_SIZE;
		m_sock.async_write_some(buffer(sendData+offSet, sendSize),
			boost::bind(&RWHandler::write_handler, this, placeholders::error));
	}

	ip::tcp::socket& GetSocket()
	{
		return m_sock;
	}

	void CloseSocket()
	{
		boost::system::error_code ec;
		m_sock.shutdown(ip::tcp::socket::shutdown_send, ec);
		m_sock.close(ec);
	}

	void SetConnId(int connId)
	{
		m_connId = connId;
	}

	int GetConnId() const
	{
		return m_connId;
	}

	void setSendData(const char* str)
	{
		if (sendData != NULL)
			delete sendData;
		sendData = new char[strlen(str) + 1];
		memset(sendData, 0, sizeof(char)*(strlen(str) + 1));
		strcpy(sendData, str);
		offSet = 0;
	}

	template<typename F>
	void SetCallBackError(F f)
	{
		m_callbackError = f;
	}

private:
	void do_read_header()
	{
		//auto self(boost::shared_from_this());
		boost::asio::async_read(m_sock,
			boost::asio::buffer(read_msg.data(), Message::header_length),
			[this](boost::system::error_code ec, std::size_t /*length*/)
		{
			if (!ec && read_msg.decode_header())
			{
				do_read_body();
			}
			else
			{
				HandleError(ec);
				return;
			}
		});
	}

	void do_read_body()
	{
		//auto self(shared_from_this());
		boost::asio::async_read(m_sock,
			boost::asio::buffer(read_msg.body(), read_msg.body_length()),
			[this](boost::system::error_code ec, std::size_t /*length*/)
		{
			if (!ec)
			{
				std::cout << "data" << read_msg.data() << std::endl;
				//room_.deliver(read_msg_);
				do_read_header();
			}
			else
			{
				HandleError(ec);
				return;
			}
		});
	}

	// �첽д������ɺ�write_handler����
	//void write_handler(const boost::system::error_code& ec, boost::shared_ptr<std::string> str)
	void write_handler(const boost::system::error_code& ec)
	{
		if (ec)
		{
			std::cout << "����ʧ��!" << std::endl;
			HandleError(ec);
		}
		else
		{
			std::cout << "�ɹ����ͣ�" << std::endl;
			//������Ϣ����������
			HandleRead();
		}
			
	}
	//�첽��������ɺ�read_handler����
	void read_handler(const boost::system::error_code& ec, boost::shared_ptr<std::vector<char> > str)
	{
		if (ec)
		{
			//std::cout << "û�н��յ���Ϣ��" << std::endl;
			HandleError(ec);
			return;
		}
		else
		{
			if ((offSet + SEND_SIZE) > strlen(sendData))
			{
				std::cout << "������ɣ�" << std::endl;
				boost::system::error_code ec;
				write(m_sock, buffer("SendFinish", 10), ec);
				//�ر�����
				CloseSocket();
				std::cout << "�Ͽ�����" << m_connId << std::endl;
				if (m_callbackError)
					m_callbackError(m_connId);
			}
			else
			{
				if (strcmp(&(*str)[0], "Ok") == 0)
				{
					std::cout << "������Ϣ��" << &(*str)[0] << std::endl;
					offSet = offSet + SEND_SIZE;
					HandleWrite();
				}
				else
				{
					std::cout << "û�н��յ����أ��ط���Ϣ!" << std::endl;
					HandleWrite();
				}
			}
		}
	}

	void HandleError(const boost::system::error_code& ec)
	{
		CloseSocket();
		std::cout << "�Ͽ�����" << m_connId << std::endl;
		//std::cout << ec.message() << std::endl;
		if (m_callbackError)
			m_callbackError(m_connId);
	}

private:
	ip::tcp::socket m_sock;
	std::array<char, MAX_IP_PACK_SIZE> m_buff;
	int m_connId;
	std::function<void(int)> m_callbackError;
	int offSet;
	char *sendData = NULL;

	char *response_data;

	Message read_msg;
};