// Copyright(c) David Sleeper(Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#include "SPPNetworkMessenger.h"
#include "SPPLogging.h"
#include <sstream>

namespace SPP
{
	static const int32_t MAX_MESSAGE_SIZE = IDEAL_NETWORK_MESSAGE_SIZE * (1 << 15);

	LogEntry LOG_NETMSG("NETMSG");

	struct PackedMessage
	{
		uint16_t isControl : 1;	// permissions for everyone else
		uint16_t MsgCountOrIndex : 15;
	};

	union ControlMessage
	{
		PackedMessage packed;
		uint16_t unpacked;
	};

	std::string MessageSplitTranscoder::ReportString()
	{
		return "MessageSplitTranscoder";
	}

	void MessageSplitTranscoder::Send(NetworkConnection *InConnection, const void *Data, int32_t DataLength, uint16_t MsgMaskInfo)
	{
		SE_ASSERT(DataLength < MAX_MESSAGE_SIZE);
		SPP_LOG(LOG_NETMSG, LOG_VERBOSE, "MessageSplitTranscoder::Send %d", DataLength);
		
		if (auto Parent = _PreviousTranscoder.lock())
		{
			uint16_t MessageCount = (DataLength + (IDEAL_NETWORK_MESSAGE_SIZE - 1)) / IDEAL_NETWORK_MESSAGE_SIZE;
						
			if (MessageCount > 1)
			{
				SPP_LOG(LOG_NETMSG, LOG_VERBOSE, "MessageSplitTranscoder::Splitting into %d messages", MessageCount);

				ControlMessage message;

				// send the initial split info
				{
					BinaryBlobSerializer MessageData;
					message.packed.isControl = 1;
					message.packed.MsgCountOrIndex = MessageCount;
					MessageData << message.unpacked;
					MessageData << DataLength;
					Parent->Send(InConnection, MessageData, (int32_t)MessageData.Size(), MsgMaskInfo);
				}

				uint8_t *DataIdx = (uint8_t*)Data;
				uint16_t msgIdx = 0;
				while (DataLength > 0)
				{
					BinaryBlobSerializer LocalMessageData;

					int32_t SendAmount = std::min(IDEAL_NETWORK_MESSAGE_SIZE, DataLength);

					message.packed.isControl = 1;
					message.packed.MsgCountOrIndex = msgIdx;
					LocalMessageData << message.unpacked;
					LocalMessageData << SendAmount;

					LocalMessageData.Write(DataIdx, SendAmount);
					Parent->Send(InConnection, LocalMessageData, (int32_t)LocalMessageData.Size(), MsgMaskInfo);

					DataIdx += SendAmount;
					DataLength -= SendAmount;
					msgIdx++;
				}

				SE_ASSERT(MessageCount == msgIdx);
			}
			else
			{
				BinaryBlobSerializer MessageData;

				ControlMessage message;
				message.packed.isControl = 0;
				message.packed.MsgCountOrIndex = 0;
				MessageData << message.unpacked;

				MessageData.Write(Data, DataLength);
				Parent->Send(InConnection, MessageData, (int32_t)MessageData.Size(), MsgMaskInfo);
			}
		}
	}

	// something higher up is passing down data
	void MessageSplitTranscoder::Recv(NetworkConnection *InConnection, const void *Data, int32_t DataLength, uint16_t MsgMaskInfo)
	{
		SPP_LOG(LOG_NETMSG, LOG_VERBOSE, "MessageSplitTranscoder::Recv %d", DataLength);

		MemoryView DataView(Data, DataLength);
		
		if (auto Child = _NextTranscoder.lock())
		{
			ControlMessage message;
			DataView >> message.unpacked;

			if (message.packed.isControl)
			{
				SPP_LOG(LOG_NETMSG, LOG_VERBOSE, "MessageSplitTranscoder::Recv has control messages");

				if (_pendingMessageCount == 0)
				{
					// total messages we should get
					_pendingMessageCount = message.packed.MsgCountOrIndex;

					int32_t TotalDataLength = 0;
					DataView >> TotalDataLength;
					_pendingAssembly.resize(TotalDataLength);

					SPP_LOG(LOG_NETMSG, LOG_VERBOSE, "MessageSplitTranscoder::Recv new split message, %d messages, total size %d", _pendingMessageCount, TotalDataLength);
				}
				else
				{
					int32_t CurrentChunkSize = 0;
					DataView >> CurrentChunkSize;
					DataView.Read(_pendingAssembly.data() + (message.packed.MsgCountOrIndex * IDEAL_NETWORK_MESSAGE_SIZE), CurrentChunkSize);

					SPP_LOG(LOG_NETMSG, LOG_VERBOSE, " - SPLIT recv messages index %d", message.packed.MsgCountOrIndex);

					// once becomes complete pass down
					if (message.packed.MsgCountOrIndex == (_pendingMessageCount - 1))
					{
						SPP_LOG(LOG_NETMSG, LOG_VERBOSE, "MessageSplitTranscoder::Recv:: - data complete");

						Child->Recv(InConnection, _pendingAssembly.data(), (int32_t)_pendingAssembly.size(), MsgMaskInfo);

						_pendingMessageCount = 0;
						_pendingAssembly.clear();
					}
				}
			}
			else
			{
				// message smaller than the split size
				DataView.RebuildViewFromCurrent();
				Child->Recv(InConnection, DataView, (int32_t)DataView.Size(), MsgMaskInfo);
			}
		}
	}

	//////////////////////////////

	LogEntry LOG_NMSREL("NMRELIABILITY");

	//TODO report sending olds
	struct FMessageHeaderReliability
	{
		// bIsAck = 0
		struct StandardMessage
		{
			uint32_t bIsAck : 1;
			uint32_t bReliable : 1;
			uint32_t Index : 16;
		};

		//bIsAck = 1
		//bStatus = 0
		struct AckMessage
		{
			uint32_t bIsAck : 1;
			uint32_t bStatus : 1;
			uint32_t bHaveMsg : 1;
			uint32_t bAhead : 1;
			uint32_t Index : 16;
			void Default()
			{
				bIsAck = 1;
			}
		};

		//CURRENTLY UNUSED
		//bIsAck = 1
		//bStatus = 1
		struct StatusMessage
		{
			uint32_t bIsAck : 1;
			uint32_t bStatus : 1;
			uint32_t CurrentReliableIndex : 16;
			void Default()
			{
				bIsAck = 1;
				bStatus = 1;
			}
		};

		union 
		{			
			StandardMessage _standardMessage;
			AckMessage _ackMessage;
			StatusMessage _statusMessage;
			uint32_t PackedMessage = 0;
		} Data;
	};	

	template<>
	inline BinarySerializer& operator<< <FMessageHeaderReliability>(BinarySerializer &StorageInterface, const FMessageHeaderReliability& Value)
	{
		StorageInterface << Value.Data.PackedMessage;
		return StorageInterface;
	}

	template<>
	inline BinarySerializer& operator>> <FMessageHeaderReliability>(BinarySerializer &StorageInterface, FMessageHeaderReliability& Value)
	{
		StorageInterface >> Value.Data.PackedMessage;
		return StorageInterface;
	}

	
	void ReliabilityTranscoder::Report()
	{
		if (_sentCount > 0)
		{
			SPP_LOG(LOG_NMSREL, LOG_INFO, " - loss %f", ((float)_resentCount / (float)_sentCount) * 100.0f);
		}

		_sentCount = 0;
		_resentCount = 0;

		//SPP_LOG(LOG_NMSREL, LOG_INFO, " - reliable msg count %d at %d KB", _outgoingReliableMessages.size(), _currentBufferedAmount / 1024);
		//SPP_LOG(LOG_NMSREL, LOG_INFO, " - resend req %d by timer %d", _resendRequests, _resendByTimer);
	}

	std::string ReliabilityTranscoder::ReportString()
	{
		std::stringstream ss;
		ss << "- reliable msg count " << _outgoingReliableMessages.size() << "\n";
		ss << "- resend req " << _resendRequests << "by timer " << _resendByTimer << "\n";
		ss << "- current buffered amount " << _currentBufferedAmount / 1024 << "KB \n";
		ss << "- _reliablesAwaitingAck " << _reliablesAwaitingAck << "\n";
		return ss.str();
	}

	// sending back to front
	void ReliabilityTranscoder::Send(NetworkConnection *InConnection, const void *Data, int32_t DataLength, uint16_t MsgMaskInfo)
	{		
		if (auto Parent = _PreviousTranscoder.lock())
		{
			bool bIsReliable = (MsgMaskInfo & EMessageMask::IS_RELIABLE) != 0;

			FMessageHeaderReliability Header;
			Header.Data._standardMessage.bReliable = bIsReliable;
			Header.Data._standardMessage.Index = bIsReliable ? _outGoingIndices.Reliable++ : _outGoingIndices.Unreliable++;

			BinaryBlobSerializer MessageData;
			MessageData << Header;
			MessageData.Write(Data, DataLength);

			SPP_LOG(LOG_NMSREL, LOG_VERBOSE, "ReliabilityTranscoder::Send Reliable %d, Size %d, Idx %d", bIsReliable, DataLength, Header.Data._standardMessage.Index);

			if (bIsReliable == false && InConnection->IsSaturated() == false)
			{
				Parent->Send(InConnection, MessageData, (int32_t)MessageData.Size(), MsgMaskInfo);
			}			

			if (bIsReliable)
			{
				std::unique_ptr< StoredMessage > NewReliable = std::unique_ptr< StoredMessage >(new StoredMessage(Header.Data._standardMessage.Index, MessageData.GetArray()));
				_currentBufferedAmount += NewReliable->MemSize();
				_reliablesAwaitingAck++;
				_outgoingReliableMessages.push_back(std::move(NewReliable));
			}
		}
	}

#define RELIABLE_BEHIND_THRESHOLD 17000

	// since it does wrap around lets hope we aren't 32k messages behind
	int32_t DistanceToIndex(uint16_t NewIdx, uint16_t CurrentIdx)
	{
		int32_t ValueA = (int32_t)NewIdx - (int32_t)CurrentIdx;

		if (ValueA > std::numeric_limits< uint16_t >::max() / 2)
		{
			ValueA -= (std::numeric_limits< uint16_t >::max() + 1);
		}
		if (ValueA < -(std::numeric_limits< uint16_t >::max() / 2))
		{
			ValueA += (std::numeric_limits< uint16_t >::max() + 1);
		}

		return ValueA;
	}


	// something higher up is passing down data
	void ReliabilityTranscoder::Recv(NetworkConnection *InConnection, const void *Data, int32_t DataLength, uint16_t MsgMaskInfo)
	{
		SPP_LOG(LOG_NMSREL, LOG_VERBOSE, "ReliabilityTranscoder::Recv (%s) size: %d", InConnection->ToString().c_str(), DataLength );

		MemoryView DataView(Data, DataLength);

		FMessageHeaderReliability Header;
		DataView >> Header;

		// pass to processor
		DataView.RebuildViewFromCurrent();

		if (Header.Data._ackMessage.bIsAck)
		{
			SPP_LOG(LOG_NMSREL, LOG_VERBOSE, "ACK::Recv ID %d", Header.Data._ackMessage.Index);
			
			if (auto Parent = _PreviousTranscoder.lock())
			{
				// The other side is letting us know the last good packet it received. 
				if (Header.Data._ackMessage.bHaveMsg)
				{
					SPP_LOG(LOG_NMSREL, LOG_VERBOSE, " - positive ack");
					_lastRecvAckTime = HighResClock::now();

					// no incrementing we either are removing or break out
					for (auto it = _outgoingReliableMessages.begin(); it != _outgoingReliableMessages.end();)
					{
						int32_t IdxDelta = DistanceToIndex((*it)->Index, Header.Data._ackMessage.Index);
												
						if (IdxDelta <= 0)
						{
							//We rcv a ACK with a higher index, clear up all the older _outgoingReliableMessages
							SPP_LOG(LOG_NMSREL, LOG_VERBOSE, " - clearing confirmed id: %d delta: %d", (*it)->Index, IdxDelta);
							_reliablesAwaitingAck--;	
							_currentBufferedAmount -= (*it)->MemSize();

							_sendHealth += (float)((*it)->SendCount - 1) / 30.0f;
							it = _outgoingReliableMessages.erase(it);							
							lastRecvReliableIdx = Header.Data._ackMessage.Index;								
						}
						else
						{
							//We reached the next stored message for which we are still waiting the ack
							break;
						}
					}
					
					for (auto it = _outgoingReliableMessages.begin(); it != _outgoingReliableMessages.end(); ++it)
					{
						int32_t IdxDelta = DistanceToIndex((*it)->Index, Header.Data._ackMessage.Index);
						std::chrono::milliseconds TaskDuration = std::chrono::duration_cast<std::chrono::milliseconds>(_lastRecvAckTime - (*it)->_lastSendTime);						
						// does this need to exist?
						if (TaskDuration.count() > 300)
						{							
							(*it)->bHasSent = false;
							_resendByTimer++;
						}
					}
				}

				// they want one...
				if (Header.Data._ackMessage.bAhead)
				{
					int32_t AheadDelta = DistanceToIndex(_outGoingIndices.Reliable, Header.Data._ackMessage.Index);
					SPP_LOG(LOG_NMSREL, LOG_VERBOSE, " - negative ack %d current head %u (%u)", Header.Data._ackMessage.Index, _outGoingIndices.Reliable, AheadDelta);

					//InConnection->DegradeConnection();

					for (auto it = _outgoingReliableMessages.begin(); it != _outgoingReliableMessages.end(); )
					{
						int32_t IdxDelta = DistanceToIndex((*it)->Index, Header.Data._ackMessage.Index);
						
						if (IdxDelta < 0)
						{						
							_currentBufferedAmount -= (*it)->MemSize();
							it = _outgoingReliableMessages.erase(it);
						}
						else
						{
							(*it)->bHasSent = false;
							_resendRequests++;
							it++;
						}
					}
				}
			}

			SPP_LOG(LOG_NMSREL, LOG_VERBOSE, " - reliability state at IDX: %d with %d left", Header.Data._ackMessage.Index, _outgoingReliableMessages.size());

			return;
		}


		// 
		if (auto Child = _NextTranscoder.lock())
		{
			if (Header.Data._standardMessage.bReliable)
			{				
				int32_t IdxDelta = DistanceToIndex(Header.Data._standardMessage.Index, _incomingIndices.Reliable);

				SPP_LOG(LOG_NMSREL, LOG_VERBOSE, "STANDARD RELIABLE MSG::Recv ID %d DELTA: %d", Header.Data._standardMessage.Index, IdxDelta);
				
				// expected
				if (IdxDelta == 0)
				{
					// send a message that we received the reliable packet
					if (DataView.Size() > 0)
					{
						Child->Recv(InConnection, DataView, (int32_t)DataView.Size(), MsgMaskInfo | EMessageMask::IS_RELIABLE);
					}

					// set next index
					_incomingIndices.Reliable++;
				}				
				else if ( std::abs(IdxDelta) > RELIABLE_BEHIND_THRESHOLD)
				{
					//DC something super bad
					SPP_LOG(LOG_NMSREL, LOG_WARNING, "SOMETHING BAD");
					InConnection->CloseDown("Reliable behind max threshold!!!");
					return;
				}
				// newer than expected
				else if (IdxDelta > 0)
				{
					SPP_LOG(LOG_NMSREL, LOG_VERBOSE, " - reliable ahead %u, at %u : (%d), asking for resends", Header.Data._standardMessage.Index, _incomingIndices.Reliable, IdxDelta);

					// this approach needed?!
					if (auto Parent = _PreviousTranscoder.lock())
					{
						FMessageHeaderReliability AckMessageOut;
						AckMessageOut.Data._ackMessage.Default();
						AckMessageOut.Data._ackMessage.bAhead = true;
						AckMessageOut.Data._ackMessage.Index = _incomingIndices.Reliable;
						BinaryBlobSerializer MessageData;
						MessageData << AckMessageOut;
						Parent->Send(InConnection, MessageData, (int32_t)MessageData.Size(), 0);
					}
				}
				else if (IdxDelta < 0)
				{
					SPP_LOG(LOG_NMSREL, LOG_VERBOSE, " - Sever sending old messages, delta (%d) : not necessarily a problem", IdxDelta);
				}
			}
			else
			{
				int32_t IdxDelta = DistanceToIndex(_incomingIndices.Unreliable, Header.Data._standardMessage.Index);

				SPP_LOG(LOG_NMSREL, LOG_VERBOSE, "STANDARD *UNRELIABLE* MSG::Recv ID %d DELTA: %d", Header.Data._standardMessage.Index, IdxDelta);

				// if its ahead process it
				if (IdxDelta >= 0)
				{
					Child->Recv(InConnection, DataView, (int32_t)DataView.Size(), MsgMaskInfo);
					// set next index
					_incomingIndices.Unreliable = Header.Data._standardMessage.Index + 1;
				}
			}
		}
	}

	void ReliabilityTranscoder::Update(NetworkConnection *InConnection)
	{
		auto CurrentTime = HighResClock::now();

		if (auto Parent = _PreviousTranscoder.lock())
		{
			for (auto &Message : _outgoingReliableMessages)
			{				
				int32_t IdxDelta = DistanceToIndex(Message->Index, lastRecvReliableIdx);
		
				if (Message->bHasSent == false)
				{
					if (InConnection->IsSaturated())
					{
						break;
					}

					if (Message->SendCount)
					{
						_resentCount++;
					}

					_sentCount++;					

					Message->_lastSendTime = CurrentTime;
					Parent->Send(InConnection, Message->Message.data(), (int32_t)Message->Message.size(), EMessageMask::IS_RELIABLE);
					Message->SendCount++;				
					Message->bHasSent = true;
					lastSentReliableIdx = Message->Index;
				}					
			}

			FMessageHeaderReliability AckMessageOut;
			AckMessageOut.Data._ackMessage.Default();
			AckMessageOut.Data._ackMessage.bHaveMsg = true;
			AckMessageOut.Data._ackMessage.Index = (_incomingIndices.Reliable - 1);
			BinaryBlobSerializer MessageData;
			MessageData << AckMessageOut;
			Parent->Send(InConnection, MessageData, (int32_t)MessageData.Size(), 0);
		}
	}

}