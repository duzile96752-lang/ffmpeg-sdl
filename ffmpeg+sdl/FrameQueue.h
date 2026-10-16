#pragma once
#include<string>
extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
}
#include<queue>
#include<mutex>
#include<thread>
#include<condition_variable>
#include<cstdint>
#include<vector>

class FrameQueue {
private:
	std::queue<AVFrame*> q_;
	mutable std::mutex mtx_;// 保护 q_ 和 aborted_
	std::condition_variable cvNotFull_;// "队列没满" 条件
	std::condition_variable cvNotEmpty_;// "队列非空" 条件
	size_t maxSize_;// 队列最大容量
	bool aborted_ = false;
public:
	explicit FrameQueue(size_t maxSize) :maxSize_(maxSize) {}

	bool push(AVFrame* frame);// 生产者调用
	AVFrame* pop();// 消费者调用
	void abort();// 强制中止：清空队列、唤醒所有等待
	bool isAborted()const;
};

struct AudioFrame {
	std::vector<uint8_t> data;//s16 interleaved PCM
	double pts = 0.0;
};

class AudioFrameQueue {
private:
	std::queue<AudioFrame> q_;
	mutable std::mutex mtx_;
	std::condition_variable cvNotFull_;
	std::condition_variable cvNotEmpty_;
	size_t maxSize_;
	bool aborted_ = false;
public:
	explicit AudioFrameQueue(size_t maxSize) :maxSize_(maxSize) {}
	//生产者
	bool push(AudioFrame&& frame) {
		std::unique_lock<std::mutex> lock(mtx_);
		cvNotFull_.wait(lock, [this] {
			return aborted_ || q_.size() < maxSize_;
			});
		if (aborted_)return false;
		q_.push(std::move(frame));
		cvNotEmpty_.notify_one();
		return true;
	}

	//消费者
	bool pop(AudioFrame& out) {
		std::unique_lock<std::mutex> lock(mtx_);
		cvNotEmpty_.wait(lock, [this] {
			return aborted_ || !q_.empty();
			});
		if (q_.empty())return false;
		out = std::move(q_.front());
		q_.pop();
		cvNotFull_.notify_one();
		return true;
	}

	void abort() {
		std::lock_guard<std::mutex> lock(mtx_);
		aborted_ = true;
		while (!q_.empty()) q_.pop();
		cvNotFull_.notify_all();
		cvNotEmpty_.notify_all();
	}

	bool isAborted()const {
		std::lock_guard<std::mutex>lock(mtx_);
		return aborted_;
	}
};