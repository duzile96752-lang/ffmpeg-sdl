#pragma once
#include<cstdint>
#include<vector>
#include<chrono>

struct SDL_AudioStream;

class AudioPlayer {
private:
	SDL_AudioStream* stream_ = nullptr;
	int sampleRate_ = 48000;//采样率
	int channels_ = 2;//声道数
	int64_t pushedBytes_ = 0;//累计提交的字节数
	double lastPts_ = 0.0;   // 最后一次 push 的音频帧 PTS
public:
	AudioPlayer();
	~AudioPlayer();

	//打开音频设备：sampleRate采样率，channels声道数
	//输出格式固定为S16 interleaved
	bool open(int sampleRate, int channels);
	//提交PCM数据（S16 interleaved）
	void push(const uint8_t* data, int size);
	void push(const std::vector<uint8_t>& data,double pts=-1.0);
	//SDL内部队列中还有多少字节没播
	int queueBytes()const;
	//已播放时间：（已提交字节 - 队列剩余字节）/ 每秒字节数
	//用作音频时钟
	double playedSeconds()const;
	//还有多少时间没有播放
	double queueSeconds()const;
	//队列中还有多少秒的数据没有播
	void pause(bool b);
	//清空SDL内部队列
	void clear();
};