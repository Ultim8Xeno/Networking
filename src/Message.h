#pragma once
#include "Common.h"

namespace Network {

	template<typename T>
	struct MessageHeader
	{
		T id{};
		uint32_t size = 0;
	};

	template<typename T>
	struct Message
	{
		MessageHeader<T> header{};
		std::vector<uint8_t> body;

		friend std::ostream& operator <<(std::ostream& os, const Message<T>& msg)
		{
			os << "ID: " << uint32_t(header.ID) << ", Size: " << header.size;
			return os;
		}

		template<typename DataType>
		friend Message<T>& operator <<(Message<T>& msg, const DataType& data)
		{
			uint32_t i = msg.header.size;
			msg.body.resize(i + sizeof(DataType));
			std::memcpy(msg.body.data() + i, &data, sizeof(DataType));

			msg.header.size = msg.body.size();
			return msg;
		}

		template<typename DataType>
		friend Message<T>& operator >>(Message<T>& msg, DataType& data)
		{
			uint32_t i = msg.header.size - sizeof(DataType);
			std::memcpy(&data, msg.body.data() + i, sizeof(DataType));
			msg.body.resize(i);

			msg.header.size = msg.body.size();
			return msg;
		}
	};

	template<typename T>
	class Connection;

	template<typename T>
	struct OwnedMessage
	{
		std::shared_ptr<Connection<T>> remote = nullptr;
		Message<T> msg;
	};

	template<typename T>
	class TsQueue
	{
	public:
		TsQueue() = default;
		TsQueue(const TsQueue<T>&) = delete;
		virtual ~TsQueue() { Clear(); }

		T& Front()
		{
			std::scoped_lock lk(m_QueueMutex);
			return m_Queue.front();
		}
		T& Back()
		{
			std::scoped_lock lk(m_QueueMutex);
			return m_Queue.back();
		}
		void PushFront(const T& item)
		{
			std::scoped_lock lk(m_QueueMutex);
			m_Queue.emplace_front(std::move(item));
			
			std::unique_lock<std::mutex> ul(m_WaitingMutex);
			m_WaitUntilNotEmpty.notify_one();
		}
		void PushBack(const T& item)
		{
			std::scoped_lock lk(m_QueueMutex);
			m_Queue.emplace_back(std::move(item));

			std::unique_lock<std::mutex> ul(m_WaitingMutex);
			m_WaitUntilNotEmpty.notify_one();
		}
		T PopFront()
		{
			std::scoped_lock lk(m_QueueMutex);
			T item = std::move(m_Queue.front());
			m_Queue.pop_front();
			return item;
		}
		T PopBack()
		{
			std::scoped_lock lk(m_QueueMutex);
			T item = std::move(m_Queue.back());
			m_Queue.pop_back();
			return item;
		}
		bool Empty()
		{
			std::scoped_lock lk(m_QueueMutex);
			return m_Queue.empty();
		}
		void Clear()
		{
			std::scoped_lock lk(m_QueueMutex);
			m_Queue.clear();
		}
		void Wait()
		{
			while (Empty())
			{
				std::unique_lock<std::mutex> ul(m_WaitingMutex);
				m_WaitUntilNotEmpty.wait(ul);
			}
		}
	private:
		std::deque<T> m_Queue;
		std::mutex m_QueueMutex;

		std::condition_variable m_WaitUntilNotEmpty;
		std::mutex m_WaitingMutex;
	};
}