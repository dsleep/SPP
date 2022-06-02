// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#include "SPPVideo.h"
#include "SPPLogging.h"
#include "SPPString.h"

#if _WIN32

	#include "Windows.h"
	

	#pragma warning(disable : 4244)

#endif

extern "C"
{

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
#include <libavutil/mathematics.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
}

SPP_OVERLOAD_ALLOCATORS

namespace SPP
{
	LogEntry LOG_VIDEO("LOG_VIDEO");
	LogEntry LOG_LAV("libav");
		
	struct RRMpegFileWriter::Impl
	{
		AVFormatContext *oc = NULL;
		AVStream *vs = NULL;
		int nFps = 0;
		uint64_t FrameCount = 0;
	};


	void InitVideoLib()
	{
		static bool bIsReady = false;

		if (bIsReady == false)
		{
			bIsReady = true;
			const AVCodec* codec = nullptr;
			void* i = nullptr;
			while ((codec = av_codec_iterate(&i)))
			{
				SPP_LOG(LOG_LAV, LOG_INFO, "Found LIBAV Codec: %s type %s", codec->name, av_codec_is_encoder(codec) ? "encoder" : "decoder");				
			}

			AVHWDeviceType type = AV_HWDEVICE_TYPE_NONE;
			while ((type = av_hwdevice_iterate_types(type)) != AV_HWDEVICE_TYPE_NONE)
			{
				SPP_LOG(LOG_LAV, LOG_INFO, "Found LIBAV Hardware type %s", av_hwdevice_get_type_name(type));
			}
		}
	}

	RRMpegFileWriter::RRMpegFileWriter(const std::string &FileName, bool IsH265, int32_t Width, int32_t Height, int32_t InFPS) : _impl(new Impl())
	{
		InitVideoLib();

		//_impl->oc = avformat_alloc_context();
		//_impl->nFps = InFPS;

		//// Set format on oc
		//AVOutputFormat *fmt = av_guess_format("mpegts", NULL, NULL);
		//fmt->video_codec = IsH265 ? AV_CODEC_ID_HEVC : AV_CODEC_ID_H264;

		//_impl->oc->oformat = fmt;
		////_impl->oc->url = av_strdup(szInFilePath);
		////LOG(INFO) << "Streaming destination: " << oc->url;

		//// Add video stream to oc
		//_impl->vs = avformat_new_stream(_impl->oc, NULL);

		//_impl->vs->id = 0;

		//// Set video parameters
		//auto *vpar = _impl->vs->codec;
		//vpar->codec_id = fmt->video_codec;
		//vpar->codec_type = AVMEDIA_TYPE_VIDEO;
		//vpar->width = Width;
		//vpar->height = Height;

		//// Everything is ready. Now open the output stream.
		//if (avio_open(&_impl->oc->pb, FileName.c_str(), AVIO_FLAG_WRITE) < 0) {
		//}

		//// Write the container header
		//if (avformat_write_header(_impl->oc, NULL)) {

		//}
	}
	RRMpegFileWriter::~RRMpegFileWriter()
	{
		if (_impl->oc)
		{
			av_write_trailer(_impl->oc);
			avio_close(_impl->oc->pb);
			avformat_free_context(_impl->oc);
		}
	}

	void RRMpegFileWriter::Seek(int64_t DataPos)
	{
	}
	int64_t RRMpegFileWriter::Tell() const
	{
		return 0;
	}
	int64_t RRMpegFileWriter::Size() const
	{
		return 0;
	}
	bool RRMpegFileWriter::Write(const void *Data, int64_t DataLength)
	{
		AVPacket pkt = { 0 };
		av_init_packet(&pkt);
		pkt.pts = av_rescale_q(_impl->FrameCount++, AVRational{ 1, _impl->nFps }, _impl->vs->time_base);
		// No B-frames
		pkt.dts = pkt.pts;
		pkt.stream_index = _impl->vs->index;
		pkt.data = (uint8_t*)Data;
		pkt.size = DataLength;

		if (!memcmp(Data, "\x00\x00\x00\x01\x67", 5)) {
			pkt.flags |= AV_PKT_FLAG_KEY;
		}

		// Write the compressed frame into the output
		int ret = av_write_frame(_impl->oc, &pkt);
		av_write_frame(_impl->oc, NULL);
		if (ret < 0) {
			SPP_LOG(LOG_LAV, LOG_INFO, "av_write_frame failed!!!");
		}

		return true;
	}
	bool RRMpegFileWriter::Read(void *Data, int64_t DataLength)
	{
		return true;
	}

	class AVIOMemoryContext
	{
	private:
		const uint8_t *_memory = nullptr;
		const int64_t _memorySize = 0;
		int64_t currentPosition = 0;
		AVIOContext * ctx_ = nullptr;

	public:
		AVIOMemoryContext(const uint8_t *memory, int64_t DataSize) : _memory(memory), _memorySize(DataSize)
		{
			int32_t bufferSize = 16 * 1024;
			ctx_ = ::avio_alloc_context(
				(unsigned char*)av_malloc(bufferSize),
				bufferSize,
				0,
				this,
				&AVIOMemoryContext::read,
				NULL,
				&AVIOMemoryContext::seek);
			SE_ASSERT(ctx_);
		}

		~AVIOMemoryContext()
		{
			av_freep(&ctx_->buffer);
			avio_context_free(&ctx_);
		}

		int64_t _tell() const
		{
			return currentPosition;
		}

		int _read(unsigned char *buf, int buf_size)
		{
			auto readAmmount = std::max<int64_t>(std::min<int64_t>(_memorySize - currentPosition, buf_size), 0);
			if (readAmmount)
			{
				memcpy(buf, _memory + currentPosition, readAmmount);
				currentPosition += readAmmount;
			}
			SE_ASSERT(currentPosition >= 0 && currentPosition <= _memorySize);
			return readAmmount;
		}

		int64_t _seek(int64_t offset, int whence)
		{
			if (0x10000 == whence)
			{
				return _memorySize;
			}
			if (whence == SEEK_SET)
			{
				currentPosition = offset;
			}
			else if (whence == SEEK_CUR)
			{
				currentPosition += offset;
			}
			else if (whence == SEEK_END)
			{
				currentPosition = _memorySize + offset;
			}
			else
			{
				SE_ASSERT(false);
			}
			SE_ASSERT(currentPosition >= 0 && currentPosition <= _memorySize);
			return currentPosition;
		}

		static int read(void *opaque, unsigned char *buf, int buf_size)
		{
			AVIOMemoryContext* h = static_cast<AVIOMemoryContext*>(opaque);
			return h->_read(buf, buf_size);
		}		

		static int64_t seek(void *opaque, int64_t offset, int whence)
		{
			AVIOMemoryContext* h = static_cast<AVIOMemoryContext*>(opaque);
			return h->_seek(offset, whence);			
		}

		AVIOContext *get_avio()
		{
			return ctx_;
		}
	};

	struct RRMpegFileReader::Impl
	{
		std::shared_ptr<AVIOMemoryContext> memoryContext;
		AVFormatContext *ctx = nullptr;
		AVCodecContext *c = nullptr;
		AVCodecParserContext *parser = nullptr;		
		AVPacket *pkt = nullptr;
	};

	RRMpegFileReader::RRMpegFileReader(const uint8_t *InData, int64_t DataSize) : _impl(new Impl()), _data(InData), _dataSize(DataSize) 
	{
		InitVideoLib();

		//_impl->memoryContext = std::make_shared< AVIOMemoryContext >(InData, DataSize);
		//_impl->ctx = avformat_alloc_context();
		//_impl->ctx->pb = _impl->memoryContext->get_avio();

		//int ret = 0;
		//if ((ret = avformat_open_input(&_impl->ctx, "memory", NULL, NULL)) < 0)
		//{
		//	SPP_LOG(LOG_LAV, LOG_INFO, "avformat_open_input failed!!!");
		//	return;
		//}
		//if ((ret = avformat_find_stream_info(_impl->ctx, NULL)) < 0)
		//{
		//	SPP_LOG(LOG_LAV, LOG_INFO, "avformat_find_stream_info failed!!!");
		//	return;
		//}

		//for (int32_t Iter = 0; Iter < (int32_t)_impl->ctx->nb_streams; Iter++)
		//{
		//	if (_impl->ctx->streams[Iter]->codec &&
		//		_impl->ctx->streams[Iter]->codec->codec_type == AVMEDIA_TYPE_VIDEO)
		//	{
		//		// found video stream
		//		SPP_LOG(LOG_LAV, LOG_INFO, "Found Video Stream");
		//		SPP_LOG(LOG_LAV, LOG_INFO, " - Width: %d", _impl->ctx->streams[Iter]->codec->width);
		//		SPP_LOG(LOG_LAV, LOG_INFO, " - Height: %d", _impl->ctx->streams[Iter]->codec->height);
		//		SPP_LOG(LOG_LAV, LOG_INFO, " - Frames: %d", _impl->ctx->streams[Iter]->codec_info_nb_frames);

		//		_impl->c = _impl->ctx->streams[Iter]->codec;
		//		_impl->parser = av_parser_init(_impl->ctx->streams[Iter]->codec->codec_id);

		//		break;
		//	}
		//} 

		//_impl->pkt = av_packet_alloc();
	}
		

	RRMpegFileReader::~RRMpegFileReader()
	{
		//restore to null since we handle it in memory shutdown
		_impl->ctx->pb = nullptr;
		avformat_close_input(&_impl->ctx);

		if (_impl->parser)
		{
			av_parser_close(_impl->parser);
		}
		
		if(_impl->pkt)
		{
			av_packet_free(&_impl->pkt);
		}
		
		if (_impl->ctx)
		{
			av_freep(&_impl->ctx);
		}
	}

	void RRMpegFileReader::GetNextFrame(std::vector<uint8_t> &outData, int64_t &oSize)
	{
		oSize = 0;
		auto ret = av_read_frame(_impl->ctx, _impl->pkt);

		if (ret < 0)
		{
			return;
		}
		
		if (_impl->pkt->size > 0)
		{
			oSize = _impl->pkt->size;
			if ((int64_t)outData.size() < oSize)
			{
				outData.resize(oSize);
			}

			memcpy(outData.data(), _impl->pkt->data, oSize);
		}
	}

	
	class LibAVCodecHandler
	{
	protected:
		SwsContext *convertCtx = nullptr;
		const AVCodec* _codec = NULL;
		AVCodecContext* _codecContext = NULL;
		AVCodecParserContext *_parser = nullptr;
		AVFrame *yuvpic = nullptr;
		AVFrame *rgbpic = nullptr;
		AVPacket *pkt = nullptr;
		int32_t _width = -1;
		int32_t _height = -1;
		int32_t _fps = 2;
		int32_t _latency = 5; //secs?
		int32_t _bitrate = 40000;
		int linesize[AV_NUM_DATA_POINTERS] = { 0 };
		bool bIsEncoder = false;
		int32_t frameCounter = 0;

	public:
		LibAVCodecHandler(int32_t FrameWidth, int32_t FrameHeight, bool _bencoder, int32_t fps = 2, int32_t latency = 5, int32_t bitrate = 40000)
			: _width(FrameWidth)
			, _height(FrameHeight)
			, bIsEncoder(_bencoder)
			, _fps(fps)
			, _latency(latency)
			, _bitrate(bitrate)
		{
			InitVideoLib();
			
			if (bIsEncoder)
			{
#if WITH_CUDA				
				_codec = avcodec_find_encoder_by_name("hevc_nvenc");
#else
				_codec = avcodec_find_encoder(AV_CODEC_ID_HEVC);
#endif
			}
			else
			{
				_codec = avcodec_find_decoder(AV_CODEC_ID_HEVC);
			}

			SE_ASSERT(_codec);		
			_parser = av_parser_init(_codec->id); //codec was nullptr
			
			SPP_LOG(LOG_LAV, LOG_INFO, "USING Codec: %s", _codec->name);
			
			_codecContext = avcodec_alloc_context3(_codec);
			SE_ASSERT(_codecContext);
			
			//deprecated
			//avcodec_get_context_defaults3(_codecContext, _codec);
			_codecContext->codec = _codec;			
			_codecContext->codec_id = _codec->id;
			_codecContext->codec_type = _codec->type;

			av_opt_show2(_codecContext->priv_data, NULL, bIsEncoder ? AV_OPT_FLAG_ENCODING_PARAM : AV_OPT_FLAG_DECODING_PARAM, 0);
						
			_codecContext->bit_rate = 120000;
			_codecContext->width = _width;
			_codecContext->height = _height;
			_codecContext->time_base = { 1, _fps }; // {1,4} 
			_codecContext->framerate = { _fps, 1 };
			_codecContext->pix_fmt = AV_PIX_FMT_YUV420P;

			if (bIsEncoder)
			{
				int error;				
				
				if (error = av_opt_set(_codecContext->priv_data, "preset", "llhp", 0) != 0)
				{
					SPP_LOG(LOG_LAV, LOG_WARNING, "av_opt_set: failed llhp");
				}

				if (error = av_opt_set(_codecContext->priv_data, "zerolatency", "1", 0) != 0)
				{
					SPP_LOG(LOG_LAV, LOG_WARNING, "av_opt_set: failed zerolatency");
				}
			}
			
			auto ret = avcodec_open2(_codecContext, _codec, nullptr);
			if (ret < 0)
			{
				SPP_LOG(LOG_LAV, LOG_INFO, "avcodec_open2 error: %d", ret);
				SE_ASSERT(false);
			}

			if (bIsEncoder)
			{
				convertCtx = sws_getContext(_width, _height,
					AV_PIX_FMT_RGBA, _width, _height,
					AV_PIX_FMT_YUV420P, SWS_POINT,
					NULL, NULL, NULL); // Preparing to convert my generated RGB images to YUV frames.
			}
			else
			{
				convertCtx = sws_getContext(_width, _height,
					AV_PIX_FMT_YUV420P, _width, _height,
					AV_PIX_FMT_BGRA, SWS_POINT,
					NULL, NULL, NULL); // Preparing to convert my generated RGB images to YUV frames.
			}
			
			SE_ASSERT(convertCtx);

			linesize[0] = _width * 4;
						
			yuvpic = av_frame_alloc();
			yuvpic->format = AV_PIX_FMT_YUV420P;
			yuvpic->width = _width;
			yuvpic->height = _height;
			av_frame_get_buffer(yuvpic, 1);

			// Allocating memory for each RGB frame, which will be lately converted to YUV:
			rgbpic = av_frame_alloc();
			rgbpic->format = AV_PIX_FMT_RGBA;
			rgbpic->width = _width;
			rgbpic->height = _height;
			av_frame_get_buffer(rgbpic, 1);
						
			pkt = av_packet_alloc();
		}

		virtual ~LibAVCodecHandler()
		{
			avcodec_free_context(&_codecContext);
			av_frame_free(&yuvpic);
			av_frame_free(&rgbpic);
			av_packet_free(&pkt);
			av_parser_close(_parser);
		}

		void EncodingFrame(const void *InData, int32_t DataSize, std::function<void(const void*, int32_t)> cbFunc)
		{
			if (InData)
			{
				memcpy(rgbpic->data[0], InData, DataSize);
				sws_scale(convertCtx, rgbpic->data, rgbpic->linesize, 0, _height, yuvpic->data, yuvpic->linesize);
			}

			yuvpic->pts = frameCounter++;
			auto ret = avcodec_send_frame(_codecContext, InData ? yuvpic : nullptr);

			//SPP_LOG(LOG_LAV, LOG_VERBOSE, "yuvpic->pts %d", yuvpic->pts);

			while (ret >= 0)
			{
				ret = avcodec_receive_packet(_codecContext, pkt);
				if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
				{
					return;
				}
				else if (ret < 0)
				{
					SPP_LOG(LOG_LAV, LOG_INFO, "avcodec_send_frame error: %d", ret);
				}

				//SPP_LOG(LOG_LAV, LOG_VERBOSE, "avcodec_receive_packet size: %d", pkt->size);

				cbFunc(pkt->data, pkt->size);
				av_packet_unref(pkt);
				
				//more to free
			}
		}

		void DecodingFrame(const void *InData, int32_t DataSize, std::function<void(const void*, int32_t)> cbFunc)
		{			
			pkt->data = (uint8_t*) InData;
			pkt->size = DataSize;
			auto ret = avcodec_send_packet(_codecContext, pkt);

			if (ret < 0) 
			{
				SPP_LOG(LOG_LAV, LOG_INFO, "avcodec_send_packet err: %d", ret);
			}

			while (ret >= 0)
			{
				ret = avcodec_receive_frame(_codecContext, yuvpic);
				if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
				{
					return;
				}
				else if (ret < 0)
				{
					SPP_LOG(LOG_LAV, LOG_INFO, "avcodec_receive_frame error: %d", ret);
				}
				//SPP_LOG(LOG_LAV, LOG_INFO, "yuvpic->pts %d", yuvpic->pts);
				sws_scale(convertCtx, yuvpic->data, yuvpic->linesize, 0, yuvpic->height, rgbpic->data, rgbpic->linesize);
				cbFunc(rgbpic->data[0], _width * _height * 4);
				//anything ot free
			}
		}

		virtual void PushFrame(const void *InData, int32_t DataSize, std::function<void(const void*, int32_t)> cbFunc)
		{
			if (bIsEncoder)
			{
				EncodingFrame(InData, DataSize, cbFunc);
			}
			else
			{
				DecodingFrame(InData, DataSize, cbFunc);
			}
		}
	};
	class LibAVVideoEncoding : public VideoEncodingInterface, public LibAVCodecHandler
	{
	public:
		LibAVVideoEncoding(DataFunc FrameCB, VideoSettings InSettings, EncodingSettings InEncSettings) : VideoEncodingInterface(FrameCB, InSettings, InEncSettings), 
			LibAVCodecHandler(InSettings.width, InSettings.height, true)
		{			
		}
		virtual ~LibAVVideoEncoding()
		{
			// anything?
		}
		virtual void Encode(const void *InData, int32_t DataSize) override
		{			
			PushFrame(InData, DataSize, _frameCB);			
		}
		virtual void Finalize() override
		{
			//flush encoder
			Encode(nullptr, 0);
		}
	};

	class LibAVVideoDecoding : public VideoDecodingInterface, public LibAVCodecHandler
	{
	private:

	public:
		LibAVVideoDecoding(DataFunc FrameCB, VideoSettings InSettings) : VideoDecodingInterface(FrameCB, InSettings), 
			LibAVCodecHandler(InSettings.width, InSettings.height, false)
		{

		}
		virtual ~LibAVVideoDecoding()
		{
			// anything?
		}
		virtual void Decode(const void *InData, int32_t DataSize) override
		{
			PushFrame(InData, DataSize, _frameCB);
		}
		virtual void Finalize() override
		{
			//this correct?
			//Decode(nullptr, 0);
		}
	};
	
	
	SPP_VIDEO_API std::unique_ptr<VideoEncodingInterface> CreateVideoEncoder(DataFunc FrameCB, VideoSettings InVideoSettings, EncodingSettings InEncoderSettings)
	{
		return std::make_unique<LibAVVideoEncoding>(FrameCB, InVideoSettings, InEncoderSettings);
	}

	SPP_VIDEO_API std::unique_ptr<VideoDecodingInterface> CreateVideoDecoder(DataFunc FrameCB, VideoSettings InVideoSettings)
	{
		return std::make_unique<LibAVVideoDecoding>(FrameCB, InVideoSettings);
	}
}
