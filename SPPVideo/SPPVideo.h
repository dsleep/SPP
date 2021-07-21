// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#pragma once

#include "SPPCore.h"
#include "SPPSerialization.h"

#if _WIN32 && !defined(SPP_VIDEO_STATIC)

	#ifdef SPP_VIDEO_EXPORT
		#define SPP_VIDEO_API __declspec(dllexport)
	#else
		#define SPP_VIDEO_API __declspec(dllimport)
	#endif

#else

	#define SPP_NETWORKING_API 

#endif

#include <functional>
#include <memory>
#include <vector>
#include <list>

namespace SPP
{
	using DataFunc = std::function<void(const void*, int32_t)>;

	enum class CodecTypes
	{
		H264,
		H265
	};

	struct VideoSettings
	{
		int32_t width = 0;
		int32_t height = 0;
		int32_t channels = 0;
		int32_t bytesPerPixel = 0;
		int32_t framesPerSecond = 0;
	};

	struct EncodingSettings
	{
		int32_t bitRate = 0;
		CodecTypes codec;
	};

	/// <summary>
	/// 
	/// </summary>
	class SPP_VIDEO_API VideoInterface
	{
	protected:
		VideoSettings _settings;
		DataFunc _frameCB;

	public:
		VideoInterface(DataFunc FrameCB, VideoSettings InSettings) : _frameCB(FrameCB), _settings(InSettings) { };
		const VideoSettings& GetVideoSettings() const
		{
			return _settings;
		}

		virtual ~VideoInterface() { }
	};

	/// <summary>
	/// 
	/// </summary>
	class SPP_VIDEO_API VideoEncodingInterface : public VideoInterface
	{		
	protected:
		EncodingSettings _encoderSettings;

	public:
		VideoEncodingInterface(DataFunc FrameCB, VideoSettings InSettings, EncodingSettings InEncSettings) : VideoInterface(FrameCB, InSettings), _encoderSettings(InEncSettings) { };
				
		virtual void Encode(const void *InData, int32_t DataSize) = 0;
		virtual void Finalize() = 0;

		virtual ~VideoEncodingInterface() { };
	};

	/// <summary>
	/// 
	/// </summary>
	class SPP_VIDEO_API VideoDecodingInterface : public VideoInterface
	{		
	public:
		VideoDecodingInterface(DataFunc FrameCB, VideoSettings InSettings) : VideoInterface(FrameCB, InSettings) { };

		virtual void Decode(const void *InData, int32_t DataSize) = 0;
		virtual void Finalize() = 0;

		virtual ~VideoDecodingInterface() { };
	};

	class SPP_VIDEO_API RRMpegFileWriter : public Serializer
	{
	private:
		struct Impl;
		std::unique_ptr<Impl> _impl;

	public:
		//264 or 265 only
		RRMpegFileWriter(const std::string &FileName, bool IsH265, int32_t Width, int32_t Height, int32_t InFPS);
		virtual ~RRMpegFileWriter();

		virtual void Seek(int64_t DataPos) override;
		virtual int64_t Tell() const override;
		virtual int64_t Size() const override;
		virtual bool Write(const void *Data, int64_t DataLength) override;
		virtual bool Read(void *Data, int64_t DataLength) override;
	};

	class SPP_VIDEO_API RRMpegFileReader
	{
	private:
		struct Impl;
		std::unique_ptr<Impl> _impl;
		const uint8_t *_data;
		const int64_t _dataSize;

	public:
		RRMpegFileReader(const uint8_t *InData, int64_t DataSize);
		virtual ~RRMpegFileReader();

		void GetNextFrame(std::vector<uint8_t> &outData, int64_t &oSize);
	};

	SPP_VIDEO_API std::unique_ptr<VideoEncodingInterface> CreateVideoEncoder(DataFunc FrameCB, VideoSettings InVideoSettings, EncodingSettings InEncoderSettings);
	SPP_VIDEO_API std::unique_ptr<VideoDecodingInterface> CreateVideoDecoder(DataFunc FrameCB, VideoSettings InVideoSettings);
}
