#pragma once
#include "Connection.h"
#include "Server.h"

namespace Network {

	template<typename T>
	class Client
	{
	public:
		Client() {}
		virtual ~Client()
		{
			Disconnect();
		}

		bool Connect(const std::string& host, uint16_t port)
		{
			try
			{
				asio::ip::tcp::resolver	resolver(m_AsioContext);
				asio::ip::tcp::resolver::results_type endpoints = resolver.resolve(host, std::to_string(port));

				m_Connection = std::make_unique<Connection<T>>(Connection<T>::Owner::Client, m_AsioContext, asio::ip::tcp::socket(m_AsioContext), m_QMessagesIn);
				m_Connection->ConnectToServer(endpoints);
				m_ThreadContext = std::thread([this]() { m_AsioContext.run(); });
			}
			catch (std::exception& e)
			{
				std::cerr << "Client Exception: " << e.what() << "\n";
				return false;
			}
			return true;
		}

		void Disconnect()
		{
			if (IsConnected())
			{
				m_Connection->Disconnect();
			}
			m_AsioContext.stop();
			if (m_ThreadContext.joinable())
				m_ThreadContext.join();

			m_Connection.release();
		}

		bool IsConnected()
		{
			if (m_Connection)
			{
				return m_Connection->IsConnected();
			}
			return false;
		}

		void Send(const Message<T>& msg)
		{
			if (IsConnected())
			{
				m_Connection->Send(msg);
			}
		}

		TsQueue<OwnedMessage<T>>& Incoming()
		{
			return m_QMessagesIn;
		}

	protected:
		asio::io_context m_AsioContext;
		std::thread m_ThreadContext;
		std::unique_ptr<Connection<T>> m_Connection;

	private:
		TsQueue<OwnedMessage<T>> m_QMessagesIn;
	};
}