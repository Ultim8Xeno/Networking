#pragma once
#include "Connection.h"

namespace Network {

	template<typename T>
	class Server
	{
	public:
		Server(int port)
			: m_AsioAcceptor(m_AsioContext, asio::ip::tcp::endpoint(asio::ip::tcp::v4(), port))
		{
		}

		virtual ~Server()
		{
			Stop();
		}

		bool Start()
		{
			try
			{
				WaitForClientConnection();

				m_ThreadContext = std::thread([this]() { m_AsioContext.run(); });
			}
			catch (std::exception& e)
			{
				std::cerr << "[SERVER] Exception: " << e.what() << "\n";
				return false;
			}
			std::cout << "[SERVER] Started!\n";
			return true;
		}

		void Stop()
		{
			m_AsioContext.stop();
			if (m_ThreadContext.joinable()) m_ThreadContext.join();

			std::cout << "[SERVER] Stopped!\n";
		}

		void WaitForClientConnection()
		{
			m_AsioAcceptor.async_accept(
				[this](std::error_code ec, asio::ip::tcp::socket socket)
				{
					if (!ec)
					{
						std::cout << "[SERVER] CONNECTION INITIATED: " << socket.remote_endpoint() << "\n";

						std::shared_ptr<Connection<T>> newConnection =
							std::make_shared<Connection<T>>(Connection<T>::Owner::Server,
								m_AsioContext, std::move(socket), m_QMessagesIn);

						if (OnClientConnection(newConnection))
						{
							m_Connections.PushBack(std::move(newConnection));
							m_Connections.Back()->ConnectToClient(this, ++m_IDCounter);

							std::cout << "[" << m_Connections.Back()->GetID() << "] Connection Approved\n";
						}
						else
						{
							std::cout << "[-----] Connection Denied\n";
						}
					}
					else
					{
						std::cout << "[SERVER] New Connection Error: " << ec.message() << "\n";
					}

					WaitForClientConnection();
				}
			);
		}

		void MessageClient(std::shared_ptr<Connection<T>> client, const Message<T>& msg)
		{
			if (client && client->IsConnected())
			{
				client->Send(msg);
			}
			else
			{
				OnClientDisconnect(client);
				client.reset();
				m_Connections.erase(std::remove(m_Connections.begin(), m_Connections.end), m_Connections.end());
			}
		}

		void MessageAllClients(const Message<T>& msg, std::shared_ptr<Connection<T>> ignoreClient = nullptr)
		{
			bool invalidClient = false;
			for (auto& client : m_Connections)
			{
				if (client & client->IsConnected())
				{
					if (client != ignoreClient)
					{
						client->Send(msg);
					}
				}
				else
				{
					OnClientDisconnect(client);
					client.reset();
					invalidClient = true;
				}
			}

			if (invalidClient)
				m_Connections.erase(std::remove(m_Connections.begin(), m_Connections.end()), m_Connections.end());
		}

		void Update(uint32_t maxMessages = -1, bool waitForMessage = false)
		{
			if (waitForMessage) m_QMessagesIn.Wait();

			uint32_t messageCount = 0;
			while (messageCount < maxMessages && !m_QMessagesIn.Empty())
			{
				OwnedMessage<T> msg = m_QMessagesIn.PopFront();
				OnMessage(msg.remote, msg.msg);
				messageCount++;
			}
		}
		
		virtual void OnClientValidated(std::shared_ptr<Connection<T>> client)
		{

		}

	protected:

		virtual bool OnClientConnection(std::shared_ptr<Connection<T>> client)
		{
			return false;
		}

		virtual void OnClientDisconnect(std::shared_ptr<Connection<T>> client)
		{

		}

		virtual void OnMessage(std::shared_ptr<Connection<T>> client, Message<T>& msg)
		{

		}


	private:
		TsQueue<OwnedMessage<T>> m_QMessagesIn;
		TsQueue<std::shared_ptr<Connection<T>>> m_Connections;

		asio::io_context m_AsioContext;
		std::thread m_ThreadContext;
		asio::ip::tcp::acceptor m_AsioAcceptor;

		uint32_t m_IDCounter = 0;
	};
}