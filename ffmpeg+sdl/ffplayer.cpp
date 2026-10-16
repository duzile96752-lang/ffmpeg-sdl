#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include<iostream>
extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libavutil/time.h>
#include <libswscale/swscale.h>
#include<libavutil/log.h>
}

#include"SDL3/SDL.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <atomic>
#include <chrono>
#include"shader.h"
#include"FrameQueue.h"
#include"Decoder.h"
#include"VertexArray.h"
#include"VertexBuffer.h"
#include"Renderer.h"
#include"AudioPlayer.h"
#include <algorithm>

void decoderThreadFunc(Decoder* decoder, FrameQueue* queue);
void decoderAudioThreadFunc(Decoder* decoder, AudioFrameQueue* queue);

int main(int argc, char** argv) {
	if (argc < 2) {
		std::cout << "用法: " << argv[0] << "<视频文件>\n";
		return 1;
	}
	//1.打开编码器
	Decoder decoder(argv[1]);
	if (!decoder.open()) return -1;

	int w = decoder.width();
	int h = decoder.height();


	//2.初始化GLFW
	if (!glfwInit()) return -1;

	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	//创建窗口之前设置3.3版本的核心模式的opengl环境
	GLFWwindow* window;
	//创建窗口和上下文
	window = glfwCreateWindow(w, h, "ffplayer", NULL, NULL);
	if (!window) {
		glfwTerminate();
		return -1;
	}

	//绑定上下文
	glfwMakeContextCurrent(window);

	//绑定交换间隔为1
	glfwSwapInterval(1);

	//3.初始化GLEW
	if (glewInit() != GLEW_OK) {
		av_log(nullptr, AV_LOG_INFO, "glewInit失败\n");
		glfwDestroyWindow(window);
		glfwTerminate();
		return -1;
	}
	{
	//4.初始化渲染器
	float verts[] = {
		//  x     y     u     v
		-1.0f, -1.0f, 0.0f, 1.0f,
		 1.0f, -1.0f, 1.0f, 1.0f,
		-1.0f,  1.0f, 0.0f, 0.0f,
		 1.0f,  1.0f, 1.0f, 0.0f,
	};
	Shader shader("shader.shader");
	VertexArray vao;
	VertexBuffer vbo(verts, sizeof(verts));

	Renderer renderer;
	renderer.getData(&vao, &vbo);

	if (!renderer.init(w, h, &shader)) {
		glfwDestroyWindow(window);
		glfwTerminate();
		return -1;
	}
	//打开音频播放设备
	AudioPlayer audioPlayer;
	bool hasAudio = false;
	if (decoder.hasAudio()) {
		if (audioPlayer.open(decoder.sampleRate(), decoder.channels())) {
			hasAudio = true;
		}
		else {
			std::cout << "音频设备打开失败，公播放视频" << std::endl;
		}
	}

	//5.启动解码线程
	FrameQueue videoqueue(32);
	std::thread decoderThread(decoderThreadFunc, &decoder, &videoqueue);//视频解码
	AudioFrameQueue audioqueue(32);
	std::thread audioDecodeThread;
	std::thread audioPlayThread;

	if (hasAudio) {
		//音频解码线程：解码 -> 重采样 -> push到audioQueue
		audioDecodeThread = std::thread(decoderAudioThreadFunc, &decoder,
			&audioqueue);

		//音频播放线程：从audioqueue取PCM -》推给SDL声卡
		audioPlayThread = std::thread([&] {
			while (!audioqueue.isAborted()) {
				AudioFrame af;
				if (!audioqueue.pop(af))break;
				if (af.data.empty())break;//空帧 = 解码结束
				audioPlayer.push(af.data,af.pts);

				//限制SDL内部缓冲，避免堆积过多
				while (!audioqueue.isAborted() &&
					audioPlayer.queueSeconds() > 0.05) {
					std::this_thread::sleep_for(std::chrono::milliseconds(2));
				}
			}
			});
	}

	// 6.主渲染循环
	auto wallStart = std::chrono::steady_clock::now();//记录当前系统时间
	double lastPts = -1.0;//记录上一个帧时间

	double videoStartPts = -1.0;//视频帧时间
	double audioStartClock = -1.0;//音频帧时间

	while (!glfwWindowShouldClose(window)) {
		glfwPollEvents();

		// ============ 计时点 0：循环开始 ============
		auto t0 = std::chrono::steady_clock::now();

		AVFrame* frame = videoqueue.pop();//视频帧队列
		if (!frame) break;

		// ============ 计时点 1：pop 结束 ============
		auto t1 = std::chrono::steady_clock::now();

		double ptsSec = frame->pts * decoder.timeBase();//当前帧时间

		if (hasAudio) {
			double audioClock = audioPlayer.playedSeconds();//播放了多少时间

			if (videoStartPts < 0.0 && audioClock > 0.0) {
				videoStartPts = ptsSec;
				audioStartClock = audioClock;
			}
			double videoRel = ptsSec - videoStartPts;//视频帧播放时长
			double audioRel = audioClock - audioStartClock;//音频帧播放时长
			double diff = videoRel - audioRel;//时间差

			const double MAX_DELAY = 0.10;   // 最多睡 100ms

			if (diff > 0.03) {
				double s = std::min(diff, MAX_DELAY);   
				// [30-100]毫秒以内睡diff秒，超过100ms睡100ms，需要 #include <algorithm>
				std::this_thread::sleep_for(std::chrono::duration<double>(s));
			}
			else if (diff < -0.10) {
				av_frame_free(&frame);
				continue;
			}
		}
		else {
			if (lastPts < 0.0) {
				lastPts = ptsSec;//将记录上一帧时间的变量赋值第一帧时间
				wallStart = std::chrono::steady_clock::now();
			}
			double elapsed = std::chrono::duration<double>(
				std::chrono::steady_clock::now() - wallStart).count();//系统时间过了多久
			double target = ptsSec - lastPts;
			if (target > 0.0 && elapsed < target) {
				std::this_thread::sleep_for(
					std::chrono::duration<double>(target - elapsed));
				wallStart = std::chrono::steady_clock::now();
				lastPts = ptsSec;
			}
		}

		// ============ 计时点 2：同步结束 ============
		auto t2 = std::chrono::steady_clock::now();

		int fbW, fbH;
		glfwGetFramebufferSize(window, &fbW, &fbH);
		glViewport(0, 0, fbW, fbH);

		renderer.renderFrame(frame);

		// ============ 计时点 3：渲染结束 ============
		auto t3 = std::chrono::steady_clock::now();

		static int counter = 0;
		if (++counter % 30 == 0) {
			auto ms = [](auto a, auto b) {
				return std::chrono::duration<double, std::milli>(b - a).count();
				};
			//t0:pop前系统时间	t1:pop后系统时间	t2:同步之后的系统时		t3:渲染窗口之后的系统时
			std::cout << "pop=" << ms(t0, t1)
				<< "ms  sync=" << ms(t1, t2)
				<< "ms  render=" << ms(t2, t3) << "ms" << std::endl;
		}

		/*auto ms = [](auto a, auto b) {
			return std::chrono::duration<double, std::milli>(b - a).count();
			};
		std::cout << "pop=" << ms(t0, t1)
			<< "ms  sync=" << ms(t1, t2)
			<< "ms  render=" << ms(t2, t3) << "ms" << std::endl;*/

		av_frame_free(&frame);
		glfwSwapBuffers(window);
	}

	videoqueue.abort();
	audioqueue.abort();
	if(decoderThread.joinable()) decoderThread.join();
	if(audioDecodeThread.joinable()) audioDecodeThread.join();
	if(audioPlayThread.joinable()) audioPlayThread.join();

}

	glfwDestroyWindow(window);
	glfwTerminate();
	return 0;

}