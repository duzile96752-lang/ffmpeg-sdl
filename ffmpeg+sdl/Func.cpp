#include"Decoder.h"
#include"FrameQueue.h"
#include<iostream>
extern "C" {
#include"libavutil/frame.h"
#include <libavutil/log.h>
}
#include<iostream>
void decoderThreadFunc(Decoder* decoder, FrameQueue* queue) {
	std::cout << "Decoder 解码线程启动" << std::endl;
	int frameCount = 0;

	AVFrame* frame = nullptr;
	while (!queue->isAborted()){
		if (!decoder->decodeNextFrame(frame)) {
			std::cout << "Decoder 解码结束，共" << frameCount << "帧";
			queue->push(nullptr);
			break;
		}

		AVFrame* cloned = av_frame_clone(frame);
		if (!cloned) {
			av_log(NULL, AV_LOG_DEBUG, "Decoder av_frame_clone失败");
			queue->push(nullptr);
			break;
		}

		if (!queue->push(cloned)) {
			av_frame_free(&cloned);
			break;
		}
		++frameCount;
	}
	av_log(NULL, AV_LOG_TRACE, "Decoder 解码线程退出\n");
}

void decoderAudioThreadFunc(Decoder* decoder, AudioFrameQueue* queue) {
	std::cout << "Audio 解码线程启动" << std::endl;
	int frameCount = 0;

	while (!queue->isAborted()) {
		AudioFrame aframe;
		int ret = decoder->AudioNextFrame(aframe.data, aframe.pts);

		if (ret == 0) {
			std::cout << "Audio 解码结束，共 " << frameCount << " 帧" << std::endl;
			queue->push(AudioFrame{});//添加空帧
			break;
		}
		//推入队列
		if (!queue->push(std::move(aframe))) {
			break;
		}
		++frameCount;
	}
	av_log(NULL, AV_LOG_TRACE, "Audio 解码线程退出\n");
}