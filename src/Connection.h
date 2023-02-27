#pragma once
#include "Message.h"


namespace Network {

	template<typename T>
	class Server;

	template<typename T>
	class Connection : public std::enable_shared_from_this<Connection<T>>
	{
	public:
		enum class Owner
		{
			Server,
			Client
		};

		Connection(Owner parent, asio::io_context& asioContext, asio::ip::tcp::socket socket, TsQueue<OwnedMessage<T>>& qMessagesIn)
			: m_AsioContext(asioContext), m_Socket(std::move(socket)), m_QMessagesIn(qMessagesIn)
		{
			m_OwnerType = parent;

			if (m_OwnerType == Owner::Server)
			{
				m_HandshakeOut = uint64_t(std::chrono::system_clock::now().time_since_epoch().count());
				m_HandshakeCheck = Scramble(m_HandshakeOut);
			}
		}

		virtual ~Connection()
		{

		}

		uint32_t GetID() const
		{
			return m_ID;
		}

	public:
		void ConnectToClient(Server<T>* server, uint32_t id = 0)
		{
			if (m_OwnerType == Owner::Server)
			{
				if (m_Socket.is_open())
				{
					m_ID = id;
					WriteValidation();

					ReadValidation(server);
				}
			}
		}

		void ConnectToServer(const asio::ip::tcp::resolver::results_type& endpoints)
		{
			if (m_OwnerType == Owner::Client)
			{
				asio::async_connect(m_Socket, endpoints,
					[this](std::error_code ec, asio::ip::tcp::endpoint endpoint)
						{
							if (!ec)
							{
								ReadValidation();
							}
						}
				);
			}
		}

		void Disconnect()
		{
			if (IsConnected())
			{
				asio::post(m_AsioContext, [this]() { m_Socket.close(); });
			}
		}
		
		bool IsConnected() const
		{
			return m_Socket.is_open();
		}

		void Send(const Message<T>& msg)
		{
			asio::post(m_AsioContext,
				[this, msg]()
				{
					bool writingMessages = !m_QMessagesOut.Empty();
					m_QMessagesOut.PushBack(msg);
					if (!writingMessages) 
						WriteHeader();
				}
			);
		}

	private:

		void WriteHeader()
		{
			asio::async_write(m_Socket, asio::buffer(&m_QMessagesOut.Front().header, sizeof(MessageHeader<T>)),
				[this](std::error_code ec, std::size_t length)
				{
					if (!ec)
					{
						if (m_QMessagesOut.Front().body.size() > 0)
						{
							WriteBody();
						}
						else
						{
							m_QMessagesOut.PopFront();

							if (!m_QMessagesOut.Empty())
							{
								WriteHeader();
							}
						}
					}
					else
					{
						std::cout << "[" << m_ID << "] Write Header Fail.\n";
						m_Socket.close();
					}
				}
			);
		}

		void WriteBody()
		{
			asio::async_write(m_Socket, asio::buffer(m_QMessagesOut.Front().body.data(), m_QMessagesOut.Front().body.size()),
				[this](std::error_code ec, std::size_t length)
				{
					if (!ec)
					{
						m_QMessagesOut.PopFront();
						if (!m_QMessagesOut.Empty())
						{
							WriteHeader();
						}
					}
					else
					{
						std::cout << "[" << m_ID << "] Write Body Fail.\n";
						m_Socket.close();
					}
				}
			);
		}

		void ReadHeader()
		{
			asio::async_read(m_Socket, asio::buffer(&m_MsgTemporaryIn.header, sizeof(MessageHeader<T>)),
				[this](std::error_code ec, std::size_t length)
				{
					if (!ec)
					{
						if (m_MsgTemporaryIn.header.size > 0)
						{
							m_MsgTemporaryIn.body.resize(m_MsgTemporaryIn.header.size);
							ReadBody();
						}
						else
						{
							AddToIncomingMessageQueue();
						}
					}
					else
					{
						std::cout << "[" << m_ID << "] Read Header Fail.\n";
						m_Socket.close();
					}
				}
			);
		}

		void ReadBody()
		{
			asio::async_read(m_Socket, asio::buffer(m_MsgTemporaryIn.body.data(), m_MsgTemporaryIn.body.size()),
				[this](std::error_code ec, std::size_t length)
				{
					if (!ec)
					{
						AddToIncomingMessageQueue();
					}
					else
					{
						std::cout << "[" << m_ID << "] Read Body Fail.\n";
						m_Socket.close();
					}
				}
			);
		}

		void AddToIncomingMessageQueue()
		{
			OwnedMessage<T> msg;
			if (m_OwnerType == Owner::Server)
			{
				msg.remote = this->shared_from_this();
				msg.msg = m_MsgTemporaryIn;
				m_QMessagesIn.PushBack(msg);
			}
			else
			{
				msg.msg = m_MsgTemporaryIn;
				m_QMessagesIn.PushBack(msg);
			}

			ReadHeader();
		}

		uint64_t Scramble(uint64_t input)
		{
			uint64_t out = input ^ 0xA87F20CD4A89BB2C;
			out = (out & 0x2FF3300AA0BCDE25) >> 4 | (out & 0x8AF842BCDEAF2F02) << 4;
			return out ^ 0xFB282810FAC82093;
		}

		void WriteValidation()
		{
			asio::async_write(m_Socket, asio::buffer(&m_HandshakeOut, sizeof(uint64_t)),
				[this](std::error_code ec, std::size_t length)
				{
					if (!ec)
					{
						if (m_OwnerType == Owner::Client)
							ReadHeader();
					}
					else
					{
						m_Socket.close();
					}
				}
			);
		}

		void ReadValidation(Server<T>* server = nullptr)
		{
			asio::async_read(m_Socket, asio::buffer(&m_HandshakeIn, sizeof(uint64_t)),
				[this, server](std::error_code ec, std::size_t length)
				{
					if (!ec)
					{
						if (m_OwnerType == Owner::Server)
						{
							if (m_HandshakeIn == m_HandshakeCheck)
							{
								std::cout << "Client Validated\n";
								server->OnClientValidated(this->shared_from_this());
								ReadHeader();
							}
							else
							{
								std::cout << "Client Disconnected (Fail Validation)\n";
								m_Socket.close();
							}
						}
						else
						{
							m_HandshakeOut = Scramble(m_HandshakeIn);
							WriteValidation();
						}
					}
					else
					{
						std::cout << "Client Disconnected (ReadValidation)\n";
						m_Socket.close();
					}
				}
			);
		}

	protected:
		asio::ip::tcp::socket m_Socket;
		asio::io_context& m_AsioContext;
		TsQueue<Message<T>> m_QMessagesOut;
		TsQueue<OwnedMessage<T>>& m_QMessagesIn;
		Message<T> m_MsgTemporaryIn;
		Owner m_OwnerType;
		uint32_t m_ID;

		uint64_t m_HandshakeOut = 0;
		uint64_t m_HandshakeIn = 0;
		uint64_t m_HandshakeCheck = 0;
	};
}