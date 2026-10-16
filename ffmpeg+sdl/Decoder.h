#pragma once
#include<string>
#include<vector>
extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include<libswresample/swresample.h>
#include<libavutil/channel_layout.h>
}
#include<mutex>

class Decoder {
private:
	//视频数据
	std::string FileName = "";
	AVFormatContext* fmtCtx_ = nullptr;
	AVCodecContext* codecCtx_ = nullptr;
	AVPacket* pkt_ = nullptr;
	AVFrame* rawframe_ = nullptr;
	AVFrame* convFrame_ = nullptr;
	SwsContext* swsCtx_ = nullptr;

	int videoStreamIndex_ = -1;

	int width_ = 0, height_ = 0;
	AVPixelFormat pixFmt_ = AV_PIX_FMT_NONE;
	double timeBase_ = 0.0;
	//音频数据
	AVFormatContext* afmtCtx_ = nullptr;
	AVFrame* aframe_ = nullptr;
	AVCodecContext* acodecCtx_ = nullptr;
	AVPacket* apkt_ = nullptr;
	SwrContext* swrCtx_ = nullptr;

	int audioStreamIndex_ = -1;
	int outSampleRate_ = 48000;
	int outChannels_ = 2;
	AVSampleFormat outSampleFmt_ = AV_SAMPLE_FMT_S16;
	AVChannelLayout outChLayout_{};
	double atimeBase_ = 0.0;

	bool flushSent_ = false;
	bool eof_ = false;
	double nextPts_=0.0;
	std::mutex mtx_;
public:
	Decoder(const std::string& filename) :FileName(filename) {}
	~Decoder();

	bool open();
	bool decodeNextFrame(AVFrame*& frameOut);
	int width()  const { return width_; }
	int height() const { return height_; }
	double timeBase() const { return timeBase_; }
	//音频处理
	int AudioNextFrame(std::vector<uint8_t>& out, double& pts);
	
	int sampleRate()const { return outSampleRate_; }
	int channels()const { return outChannels_; }

public:
	bool hasAudio() const { return audioStreamIndex_ >= 0; }
private:
	AVFrame* prepareFrame(AVFrame* in);
};