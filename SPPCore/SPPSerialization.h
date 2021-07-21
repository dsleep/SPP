// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#pragma once

#include "SPPCore.h"
#include <vector>
#include <list>
#include <deque>
#include <map>
#include <string>
#include <fstream> 

namespace SPP
{
	enum class Endian
	{
		Little,
		Big
	};

	inline Endian GetPlatformEndian()
	{
		uint16_t num = 1;
		if (*(uint8_t*)&num == 1)
		{
			return Endian::Little;
		}
		return Endian::Big;
	}

	// Class membership tests
	template<class T>
	struct is_bool
		: std::integral_constant<
		bool,
		std::is_same<bool, typename std::decay<T>::type>::value ||
		std::is_same<char*, typename std::decay<T>::type>::value
		> {};

	template<typename>
	struct is_std_shared_ptr : std::false_type {};
	template<typename T>
	struct is_std_shared_ptr<std::shared_ptr<T>> : std::true_type {};
	template<typename T>
	T* dereference(std::shared_ptr<T> p) { return p.get(); }
	template<typename T>
	T* init_shared_ptr(std::shared_ptr<T> p) { return new(T); }

	template<typename>
	struct is_std_vector : std::false_type {};
	template<typename T, typename A>
	struct is_std_vector<std::vector<T, A>> : std::true_type {};

	template<typename>
	struct is_std_list : std::false_type {};
	template<typename T, typename A>
	struct is_std_list<std::list<T, A>> : std::true_type {};

	template<typename>
	struct is_std_deque : std::false_type {};
	template<typename T, typename A>
	struct is_std_deque<std::deque<T, A>> : std::true_type {};

	template<typename>
	struct is_std_map : std::false_type {};
	template<typename T, typename V, typename A>
	struct is_std_map<std::map<T, V, A>> : std::true_type {};

	template <typename T>
	void swap_endian(T& u)
	{
		union
		{
			T u;
			uint8_t u8[sizeof(T)];
		} source, dest;

		source.u = u;

		for (size_t k = 0; k < sizeof(T); k++)
			dest.u8[k] = source.u8[sizeof(T) - k - 1];

		u = dest.u;
	}

	class SPP_CORE_API Serializer
	{
	protected:
		bool _doEndianSwap = false;

	public:
		virtual bool SwapEndian() const { return _doEndianSwap; }
		virtual void SetDoEndianSwap(bool DoEndianSwap) { _doEndianSwap = DoEndianSwap; }
		virtual bool Write(const void* Data, int64_t DataLength) = 0;
		virtual bool Read(void* Data, int64_t DataLength) = 0;
		virtual void Seek(int64_t DataPos) = 0;
		virtual void Skip(int64_t SkipAmount) { Seek(Tell() + SkipAmount); }
		virtual int64_t Tell() const = 0;
		virtual int64_t Size() const = 0;
		virtual int64_t Remaining() const { return Size() - Tell(); }
	};


	class SPP_CORE_API BinarySerializer : public Serializer
	{
	protected:
		bool _doEndianSwap = false;

	public:
		template<typename PoDType>
		void WriteValue(const PoDType & Value)
		{
			static_assert(std::is_trivial<PoDType>::value && std::is_standard_layout<PoDType>::value);

			if (SwapEndian())
			{
				PoDType ValueCopy = Value;
				swap_endian(ValueCopy);
				Write((void*)&ValueCopy, sizeof(PoDType));
			}
			else
			{
				Write((void*)&Value, sizeof(PoDType));
			}
		}		

		template<typename PoDType>
		void ReadValue(PoDType& Value)
		{
			static_assert(std::is_trivial<PoDType>::value && std::is_standard_layout<PoDType>::value);

			Read((void*)&Value, sizeof(PoDType));
			if (SwapEndian())
			{
				swap_endian(Value);
			}
		}
	};

	template<typename T>
	inline BinarySerializer& operator<<(BinarySerializer& Storage, const T& Value)
	{
		Storage.WriteValue<T>(Value);
		return Storage;
	}


	template<typename T>
	inline BinarySerializer& operator>>(BinarySerializer& Storage, T& Value)
	{
		Storage.ReadValue<T>(Value);
		return Storage;
	}

	template<typename T, size_t N>
	inline BinarySerializer& operator>>(BinarySerializer& Storage, T(&Value)[N])
	{
		for (int32_t Iter = 0; Iter < N; Iter++)
		{
			Storage >> Value[Iter];
		}
		return Storage;
	}

	template<typename T, size_t N>
	inline BinarySerializer& operator<<(BinarySerializer& Storage, T(&Value)[N])
	{
		for (int32_t Iter = 0; Iter < N; Iter++)
		{
			Storage << Value[Iter];
		}
		return Storage;
	}

	template<>
	inline BinarySerializer& operator<< <std::string>(BinarySerializer& Storage, const std::string& Value)
	{
		uint32_t StringLength = (uint32_t) Value.size();
		Storage << StringLength;
		Storage.Write(Value.c_str(), StringLength);
		return Storage;
	}

	template<>
	inline BinarySerializer& operator>> <std::string>(BinarySerializer& Storage, std::string& Value)
	{
		uint32_t StringLength;
		Storage >> StringLength;

		// allocate buffer
		auto buf = std::make_unique<char[]>(StringLength + 1);
		// read data
		Storage.Read(buf.get(), StringLength);
		buf.get()[StringLength] = 0;
		// initialize string
		Value = buf.get();

		return Storage;
	}

	template <class T>
	inline BinarySerializer& operator<< (BinarySerializer& Storage, const std::vector<T>& Values)
	{
		uint32_t ArraySize = (uint32_t)Values.size();
		Storage << ArraySize;

		if (Storage.SwapEndian() == false && std::is_fundamental<T>::value)
		{
			Storage.Write(Values.data(), sizeof(T) * ArraySize);
		}
		else
		{
			for (auto& Value : Values)
			{
				Storage << Value;
			}
		}

		return Storage;
	}

	template <class T>
	inline BinarySerializer& operator>> (BinarySerializer& Storage, std::vector<T>& Values)
	{
		uint32_t ArraySize;
		Storage >> ArraySize;
		Values.resize(ArraySize);

		if (Storage.SwapEndian() == false && std::is_fundamental<T>::value)
		{
			Storage.Read(Values.data(), sizeof(T) * ArraySize);
		}
		else
		{
			for (auto& Value : Values)
			{
				Storage >> Value;
			}
		}

		return Storage;
	}

	template<class>
	struct sfinae_true : std::true_type {};

	namespace detail 
	{
		template<class T>
		static auto test_binary_writer(int)->sfinae_true<decltype(std::declval<BinarySerializer&>() << std::declval<T>(), std::declval<BinarySerializer&>())>;
		template<class T>
		static auto test_binary_writer(long)->std::false_type;

		template<class T>
		static auto test_binary_reader(int)->sfinae_true<decltype(std::declval<BinarySerializer&>() >> std::declval<T>(), std::declval<BinarySerializer&>())>;
		template<class T>
		static auto test_binary_reader(long)->std::false_type;
	} // detail::

	template<class T>
	struct has_binary_writer : decltype(detail::test_binary_writer<T>(0))
	{
	};
	template<class T>
	struct has_binary_reader : decltype(detail::test_binary_reader<T>(0))
	{
	};

	/*
	* Storage backed by a byte array
	*/
	class BinaryBlobSerializer : public BinarySerializer
	{
	protected:
		std::vector<uint8_t> _data;
		int64_t _position = 0;

	public:
		
		//TODO SETUP MOVE &&

		BinaryBlobSerializer() = default;

		BinaryBlobSerializer(const std::vector<uint8_t> &InData)
		{
			_data = InData;
		}

		virtual void Seek(int64_t DataPos) override
		{
			SE_ASSERT(DataPos >= 0);
			SE_ASSERT(DataPos <= _data.size());
			_position = DataPos;
		}
		virtual int64_t Tell() const override
		{
			return _position;
		}
		virtual int64_t Size() const override
		{
			return _data.size();
		}
		virtual bool Write(const void* Data, int64_t DataIn) override
		{
			int64_t AdditionalSpace = ((_position + DataIn) - _data.size());
			if (AdditionalSpace > 0)
			{
				_data.resize(_data.size() + AdditionalSpace);
			}
			memcpy(_data.data() + _position, Data, DataIn);
			_position += DataIn;
			return true;
		}

		virtual bool Read(void* Data, int64_t DataIn) override
		{
			SE_ASSERT((DataIn + _position) <= (int64_t)_data.size());
			memcpy(Data, _data.data() + _position, DataIn);
			_position += DataIn;
			return true;
		}

		void MakeCurrentPositionStart()
		{
			_data.erase(_data.begin(), _data.begin() + _position);
			_position = 0;
		}

		operator void* () const
		{
			return (void*)_data.data();
		}

		const void* GetData() const
		{
			return (void*)_data.data();
		}

		const std::vector<uint8_t>& GetArray() const
		{
			return _data;
		}

		std::vector<uint8_t>& GetArray() 
		{
			return _data;
		}
	};

	class FileWriter : public Serializer
	{
	private:
		std::unique_ptr<std::ofstream> _fileStream;

	public:
		//STDFileWriterStorage(const char *FileName, std::ios_base::openmode moreModes = 0)
		FileWriter(const char* FileName)
		{
			_fileStream = std::make_unique<std::ofstream>(FileName, std::ifstream::out | std::ifstream::binary);
		}

		virtual ~FileWriter()
		{
			_fileStream->close();
		}
		virtual void Seek(int64_t DataPos) override
		{
			_fileStream->seekp(DataPos);
		}
		virtual int64_t Tell() const override
		{
			return _fileStream->tellp();
		}
		virtual int64_t Size() const override
		{
			//fix this as we go?
			return 0;
		}

		virtual bool Write(const void* Data, int64_t DataLength) override
		{
			_fileStream->write((char*)Data, DataLength);
			return true;
		}

		virtual bool Read(void* Data, int64_t DataLength) override
		{
			//ASSERT?
			return true;
		}
	};


	/// <summary>
	/// Read only view of memory
	/// </summary>
	class MemoryView : public BinarySerializer
	{
	protected:
		const uint8_t* _ArrayData = nullptr;
		int64_t _Position = 0;
		int64_t _Length = 0;
	public:
		MemoryView() { }
		MemoryView(const std::vector<uint8_t>& ArrayData) : _ArrayData(ArrayData.data()), _Length(ArrayData.size()) { }
		MemoryView(const void* ArrayData, int64_t Length) : _ArrayData((uint8_t*)ArrayData), _Length(Length) { }

		void RebuildViewFromCurrent()
		{
			_ArrayData += _Position;
			_Length -= _Position;
			_Position = 0;
		}

		void RebuildViewSentLength(int64_t Length)
		{
			_ArrayData += _Position;
			_Length = Length;
			_Position = 0;
		}

		operator void* () const
		{
			return (void*)_ArrayData;
		}

		const uint8_t* GetData() const
		{
			return _ArrayData;
		}

		virtual void Seek(int64_t DataPos) override
		{
			_Position = DataPos;
		}
		virtual int64_t Tell() const override
		{
			return _Position;
		}
		virtual int64_t Size() const override
		{
			return _Length;
		}
		virtual bool Write(const void* Data, int64_t DataLength) override
		{
			SE_ASSERT(false);
			return true;
		}

		virtual bool Read(void* Data, int64_t DataLength) override
		{
			SE_ASSERT((DataLength + _Position) <= _Length);
			memcpy(Data, _ArrayData + _Position, DataLength);
			_Position += DataLength;
			return true;
		}
	};
}