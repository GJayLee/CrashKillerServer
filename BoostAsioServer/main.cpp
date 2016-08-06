//�첽��ʽ
#include <iostream>
#include <string>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost\unordered_map.hpp>
#include <boost/smart_ptr.hpp>
//����
#include <boost\date_time\gregorian\gregorian.hpp>

#include "RWHandler.h"
#include "MyHttpHandler.h"

using namespace boost::asio;
using boost::system::error_code;
using ip::tcp;

//���������
const int MaxConnectionNum = 65536;
const int MaxRecvSize = 65536;
//���÷���������ʱ��Ϊ30���ӣ�1Сʱ��
const int UPDATE_TIME = 3600;

struct CHelloWorld_Service
{
	CHelloWorld_Service(io_service &iosev):m_iosev(iosev)
		, m_acceptor(iosev, tcp::endpoint(tcp::v4(),1000)), m_timer(iosev, boost::posix_time::seconds(UPDATE_TIME))
		, m_cnnIdPool(MaxConnectionNum)
		
	{
		/*httphandler = new MyHttpHandler("9e4b010a0d51f6e020ead6ce37bad33896a00f90", "2016-07-20", "2016-07-26");
		httphandler->PostHttpRequest();
		httphandler->ParseJsonAndInsertToDatabase();*/
		
		httphandler = new MyHttpHandler();
		//���Գ�ʼ��ʹ��
		//httphandler->excuteAction();

		//�˴���http����Ӧ��Ϊÿ��һ��ʱ�䴥��,��������ע��
		//m_timer.async_wait(boost::bind(&CHelloWorld_Service::wait_handler, this));

		int current = 0;
		std::generate_n(m_cnnIdPool.begin(), MaxConnectionNum, [&current] {return ++current; });
	}
	//��������
	void Accept()
	{
		//CheckHandlers();
		std::cout << "Start Listening " << std::endl;
		std::shared_ptr<RWHandler> handler = CreateHandler();
		m_acceptor.async_accept(handler->GetSocket(), [this, handler](const boost::system::error_code& error)
		{
			if (error)
			{
				std::cout << error.value() << " " << error.message() << std::endl;
				HandleAcpError(handler, error);
			}

			m_handlers.insert(std::make_pair(handler->GetConnId(), handler));
			std::cout << "current connect count: " << m_handlers.size() << std::endl;

			//�˴���http����Ӧ��Ϊÿ��һ��ʱ�䴥��
			/*MyHttpHandler *httphandler = new MyHttpHandler("9e4b010a0d51f6e020ead6ce37bad33896a00f90", "2016-05-20", "2016-07-26");
			string errorList = httphandler->PostHttpRequest();
			httphandler->ParseJsonAndInsertToDatabase();*/

			handler->HandleRead();
			/*appKey = handler->GetAppKey();
			start_date = handler->GetStartDate();
			end_date = handler->GetEndDate();*/

			//handler->setSendData(errorList.c_str());
			//handler->HandleWrite();

			deadline_timer t(m_iosev, boost::posix_time::seconds(3));
			t.wait();

			Accept();
		});
	}
	
	//��ʼ�ȴ�����
	void start()
	{
		// ��ʼ�ȴ����ӣ���������
		boost::shared_ptr<tcp::socket> psocket(new tcp::socket(m_iosev));
		// �������¼�ֻ��error_code������������boost::bind��socket�󶨽�ȥ
		m_acceptor.async_accept(*psocket,
		boost::bind(&CHelloWorld_Service::accept_handler, this, psocket,_1));
	}

	// �пͻ�������ʱaccept_handler����
	void accept_handler(boost::shared_ptr<tcp::socket> psocket, error_code ec)
	{
		if (ec)
			return;
		// �����ȴ�����
		
		//������Ϣ����������
		boost::shared_ptr<std::vector<char> > str(new std::vector<char>(100, 0));
		psocket->async_read_some(buffer(*str), bind(&CHelloWorld_Service::read_handler, this, placeholders::error, str));

		// ��ʾԶ��IP
		std::cout << psocket->remote_endpoint().address() << std::endl;
		// ������Ϣ(������)
		boost::shared_ptr<std::string> pstr(new
			std::string("hello async world!"));
		psocket->async_write_some(buffer(*pstr),
			boost::bind(&CHelloWorld_Service::write_handler, this, pstr, _1,_2));

		/*boost::shared_ptr<std::string> pReadstr;
		psocket->async_read_some(buffer(*pReadstr),
			boost::bind(&CHelloWorld_Service::read_handler, this, _1, _2));
		std::cout << *pstr << std::endl;*/

		deadline_timer t(m_iosev, boost::posix_time::seconds(2));
		t.wait();
		start();
	}

	// �첽д������ɺ�write_handler����
	void write_handler(boost::shared_ptr<std::string> pstr, error_code ec,size_t bytes_transferred)
	{
		if (ec)
			std::cout << "����ʧ��!" <<
			std::endl;
		else
			std::cout << *pstr << " �ѷ���" <<
			std::endl;
	}

	//�첽��������ɺ�read_handler����
	void read_handler(const boost::system::error_code& ec, boost::shared_ptr<std::vector<char> > str)
	{
		if (ec)
		{
			std::cout << "û�н��յ���Ϣ��" << std::endl;
		}
		else
		{
			std::cout << "������Ϣ��" << &(*str)[0] << std::endl;
		}
	}
	/*void read_handler(boost::shared_ptr<tcp::socket> psocket, error_code ec, size_t bytes_transferred)
	{
		if (ec)
			std::cout << "û�н��յ����ݣ�" << std::endl;
		else
		{
			boost::shared_ptr<std::string> pstr;
			psocket->async_read_some(buffer(*pstr),
				boost::bind(&CHelloWorld_Service::read_handler, this, _1, _2));
			std::cout << *pstr << std::endl;
		}
	}*/

private:
	//ÿ��30���ӵ���һ��http�����������,��������Ϊ�����ǰһ��
	void wait_handler()
	{
		//��ȡ��ǰ���ں�ǰһ��
		boost::gregorian::date d(boost::gregorian::day_clock::local_day());
		//����
		string today = boost::gregorian::to_iso_extended_string(d);
		boost::gregorian::day_iterator d_iter(d);
		--d_iter;
		//����
		string yesterday = boost::gregorian::to_iso_extended_string(*d_iter);

		//�˴���http����Ӧ��Ϊÿ��һ��ʱ�䴥��
		httphandler->setStartDate(yesterday);
		httphandler->setEndDate(today);
		httphandler->excuteAction();

		m_timer.expires_at(m_timer.expires_at() + boost::posix_time::seconds(UPDATE_TIME));
		m_timer.async_wait(boost::bind(&CHelloWorld_Service::wait_handler, this));
	}

	//�����Щsocket�Ͽ�
	void CheckHandlers()
	{
		if (m_handlers.size() <= 0)
			return;
		boost::unordered_map<int, std::shared_ptr<RWHandler>>::iterator iter = m_handlers.begin();
		for (; iter != m_handlers.end();iter++)
		{
			iter->second->HandleRead();
		}
	}
	//�������Ӵ���ʱ����
	void HandleAcpError(std::shared_ptr <RWHandler> eventHanlder, const boost::system::error_code& error)
	{
		std::cout << "Error��error reason��" << error.value() << error.message() << std::endl;
		//�ر�socket���Ƴ����¼�������
		eventHanlder->CloseSocket();
		StopAccept();
	}

	void StopAccept()
	{
		boost::system::error_code ec;
		m_acceptor.cancel(ec);
		m_acceptor.close(ec);
		m_iosev.stop();
	}

	std::shared_ptr<RWHandler> CreateHandler()
	{
		int connId = m_cnnIdPool.front();
		m_cnnIdPool.pop_front();
		std::shared_ptr<RWHandler> handler = std::make_shared<RWHandler>(m_iosev);

		handler->SetConnId(connId);

		handler->SetCallBackError([this](int connId)
		{
			RecyclConnid(connId);
		});

		return handler;
	}
	//ѭ��ʹ��socket ID
	void RecyclConnid(int connId)
	{
		auto it = m_handlers.find(connId);
		if (it != m_handlers.end())
			m_handlers.erase(it);
		//std::cout << "current connect count: " << m_handlers.size() << std::endl;
		m_cnnIdPool.push_back(connId);
	}


	io_service &m_iosev;
	ip::tcp::acceptor m_acceptor;

	boost::unordered_map<int, std::shared_ptr<RWHandler>> m_handlers;
	std::list<int> m_cnnIdPool;

	deadline_timer m_timer;
	string errorList;
	MyHttpHandler *httphandler;

	string appKey;
	string start_date;
	string end_date;
};



int main()
{
	io_service iosev;

	CHelloWorld_Service sev(iosev);

	// ��ʼ�ȴ�����
	//sev.start();
	sev.Accept();

	iosev.run();

	return 0;
}