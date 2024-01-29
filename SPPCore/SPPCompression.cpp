// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#include "SPPCompression.h"
#include "zlib.h"

namespace SPP
{
	void* zipAlloc(void *q, uint32_t n, uint32_t m)
	{
		return SPP_MALLOC(n * m);
	}

	void zipFree(void* q, void* p)
	{
		SPP_FREE(p);
	}

	struct ZipBinarySerializer::Impl
	{
		//auto fileStream = std::make_unique<std::ifstream>(FileName, std::ifstream::in);
		std::unique_ptr<std::ios> fileStream;
		size_t fileSize = 0;
		bool bIsCompressing = false;

		std::vector<uint8_t> dataStream;

		z_stream zipStream{ 0 };
		std::vector<uint8_t> buffer;
		int32_t ErrorVal = 0;
		std::string ErrorMsg;

		Impl()
		{
			buffer.resize(1 * 1024 * 1024);

			zipStream.zalloc = zipAlloc;
			zipStream.zfree = zipFree;
			zipStream.opaque = (voidpf)0;
		}

		~Impl()
		{
			if (bIsCompressing) 
			{
				CompessData(nullptr, 0);
				deflateEnd(&zipStream);
			}
			else
			{
				auto ptrFileStream = dynamic_cast<std::ifstream*>(fileStream.get());
				SE_ASSERT(ptrFileStream != nullptr);

				inflateEnd(&zipStream);
			}
		}

		void InitForFile(bool bInIsCompressing, const std::string &FileName)
		{
			bIsCompressing = bInIsCompressing;

			if (bInIsCompressing)
			{
				auto err = deflateInit(&zipStream, Z_BEST_SPEED);
				fileStream = std::make_unique<std::ofstream>(FileName.c_str(), std::ofstream::out | std::ofstream::binary | std::ofstream::trunc);
			}
			else
			{
				auto err = inflateInit(&zipStream);
				auto ifileStream = std::make_unique<std::ifstream>(FileName.c_str(), std::ifstream::in | std::ifstream::binary | std::ifstream::ate);

				fileSize = ifileStream->tellg();
				ifileStream->seekg(0, std::ios::beg);
				fileStream = std::move(ifileStream);
			}
		}
		
		void CompessData(const void* InData, size_t DataSize)
		{
			auto ptrFileStream = dynamic_cast<std::ofstream*>(fileStream.get());
			SE_ASSERT(ptrFileStream != nullptr);

			zipStream.next_in = (z_const Bytef*)InData;
			zipStream.avail_in = DataSize;
			do {
				zipStream.next_out = buffer.data();
				zipStream.avail_out = buffer.size();
				// is this a closing call?
				auto err = deflate(&zipStream, InData == nullptr ? Z_FINISH : Z_NO_FLUSH);

				SE_ASSERT(err == Z_OK || err == Z_STREAM_END);
				auto totalOut = buffer.size() - zipStream.avail_out;
				if (totalOut)
				{
					ptrFileStream->write((const char*)buffer.data(), totalOut);
				}
			} while (zipStream.avail_out == 0);
		}

		void UncompressData(void* InData, size_t DataSize)
		{
			auto ptrFileStream = dynamic_cast<std::ifstream*>(fileStream.get());
			SE_ASSERT(ptrFileStream != nullptr);

			uint8_t *curDataPtr = (uint8_t*)InData;
			size_t dataStillLeft = DataSize;

			while (dataStillLeft)
			{
				if (zipStream.next_in == nullptr || zipStream.avail_in == 0)
				{
					auto maxRead = std::min(fileSize, buffer.size());
					ptrFileStream->read((char*)buffer.data(), maxRead);

					zipStream.next_in = buffer.data();
					zipStream.avail_in = maxRead;
				}

				if (zipStream.avail_in > 0)
				{
					zipStream.next_out = (Bytef*)curDataPtr;
					zipStream.avail_out = dataStillLeft;

					auto ret = inflate(&zipStream, Z_NO_FLUSH);

					auto amountWritten = (dataStillLeft - zipStream.avail_out);
					curDataPtr += amountWritten;
					dataStillLeft -= amountWritten;
					
					if (ret == Z_DATA_ERROR) 
					{
						ErrorVal = Z_DATA_ERROR;
						ErrorMsg = zipStream.msg;
						break;
					}
					if (ret == Z_STREAM_END)
					{
						break;
					}
				}
				else
				{
					break;
				}
			}			
		}
	};

	ZipBinarySerializer::ZipBinarySerializer(bool bIsCompressing, const char* FileName) : _impl(new Impl())
	{
		_impl->InitForFile(bIsCompressing, FileName);
	}

	ZipBinarySerializer::~ZipBinarySerializer()
	{

	}

	void ZipBinarySerializer::Seek(int64_t DataPos) 
	{
		SE_ASSERT(false);
	};

	int64_t ZipBinarySerializer::Tell() const 
	{
		return 0;
	};

	int64_t ZipBinarySerializer::Size() const 
	{
		return 0;
	};

	bool ZipBinarySerializer::Write(const void* Data, int64_t DataIn) 
	{
		_impl->CompessData(Data, DataIn);
		return true;
	};

	bool ZipBinarySerializer::Read(void* Data, int64_t DataIn) 
	{
		_impl->UncompressData(Data, DataIn);
		return true;
	};
}

