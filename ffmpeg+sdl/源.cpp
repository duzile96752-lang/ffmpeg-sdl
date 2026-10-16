//////// 循环外
//////double videoStartPts = -1.0;
//////double audioStartClock = -1.0;
//////
//////// 循环里
//////if (hasAudio) {
//////    double audioClock = audioPlayer.playedSeconds();
//////
//////    // 第一次拿到有效音频时钟时，记录对齐点
//////    if (videoStartPts < 0.0 && audioClock > 0.0) {
//////        videoStartPts = ptsSec;
//////        audioStartClock = audioClock;
//////    }
//////
//////    // 用相对时间比较
//////    double videoRel = ptsSec - videoStartPts;
//////    double audioRel = audioClock - audioStartClock;
//////    double diff = videoRel - audioRel;
//////
//////    if (diff > 0.005) {
//////        std::this_thread::sleep_for(std::chrono::duration<double>(diff));
//////    }
//////    // diff < -0.05：视频落后，丢帧
//////    else if (diff < -0.05) {
//////        av_frame_free(&frame);
//////        continue;
//////    }
//////}
////
////
////	//6.主渲染循环
////auto wallStart = std::chrono::steady_clock::now();
//////首先将前一帧的时间设置为0
////double lastPts = -1.0;
////
////double videoStartPts = -1.0;
////double audioStartClock = -1.0;
////
////while (!glfwWindowShouldClose(window)) {
////	glfwPollEvents();
////
////	AVFrame* frame = queue.pop();
////	if (!frame)break;
////	//当前帧的时间
////	double ptsSec = frame->pts * decoder.timeBase();
////
////	if (hasAudio) {
////		double audioClock = audioPlayer.playedSeconds();
////
////		if (videoStartPts < 0.0 && audioClock>0.0) {
////			videoStartPts = ptsSec;
////			audioStartClock = audioClock;
////		}
////		double videoRel = ptsSec - videoStartPts;
////		double audioRel = audioClock - audioStartClock;
////		double diff = videoRel - audioRel;
////		if (diff > 0.005) {
////			std::this_thread::sleep_for(
////				std::chrono::duration<double>(diff));
////		}
////		else if (diff < -0.05) {
////			av_frame_free(&frame);
////			continue;
////		}
////	}
////	else {
////		if (lastPts < 0.0) {
////			lastPts = ptsSec;
////			wallStart = std::chrono::steady_clock::now();
////		}
////		double elapsed = std::chrono::duration<double>(
////			std::chrono::steady_clock::now() - wallStart).count();
////		double target = ptsSec - lastPts;
////		if (target > 0.0 && elapsed < target) {
////			std::this_thread::sleep_for(
////				std::chrono::duration<double>(target - elapsed));
////			wallStart = std::chrono::steady_clock::now();
////			lastPts = ptsSec;
////		}
////	}
////
////	//渲染
////	int fbW, fbH;
////	glfwGetFramebufferSize(window, &fbW, &fbH);
////	glViewport(0, 0, fbW, fbH);
////
////	renderer.renderFrame(frame);
////
////	av_frame_free(&frame);
////	glfwSwapBuffers(window);
////}
//
//
//if (hasAudio) {
//	double audioClock = audioPlayer.playedSeconds();
//
//	if (videoStartPts < 0.0 && audioClock > 0.0) {
//		videoStartPts = ptsSec;
//		audioStartClock = audioClock;
//	}
//	double videoRel = ptsSec - videoStartPts;
//	double audioRel = audioClock - audioStartClock;
//	double diff = videoRel - audioRel;   // ← 顺便修掉那个 bug
//
//	if (diff > 0.005) {
//		std::this_thread::sleep_for(
//			std::chrono::duration<double>(diff));
//	}
//	else if (diff < -0.05) {
//		av_frame_free(&frame);
//		continue;
//	}
//}

