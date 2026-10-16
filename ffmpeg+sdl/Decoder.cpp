#include "Decoder.h"
#include<iostream>


Decoder::~Decoder()
{
    if (convFrame_)  av_frame_free(&convFrame_);
    if (rawframe_)   av_frame_free(&rawframe_);
    if (pkt_)     av_packet_free(&pkt_);
    if (codecCtx_)   avcodec_free_context(&codecCtx_);
    if (fmtCtx_)     avformat_close_input(&fmtCtx_);
    if (swsCtx_)     sws_freeContext(swsCtx_);

    if (swrCtx_)   swr_free(&swrCtx_);
    if (aframe_)    av_frame_free(&aframe_);
    if (apkt_)      av_packet_free(&apkt_);
    if (acodecCtx_) avcodec_free_context(&acodecCtx_);
}

bool Decoder::open()
{
    if (avformat_open_input(&fmtCtx_, FileName.c_str(), nullptr, nullptr) < 0) {
        std::cout << "无法打开文件" << FileName << std::endl;
        return false;
    }
    if (avformat_find_stream_info(fmtCtx_, nullptr) < 0) {
        std::cout << "无法找到流信息" << std::endl;;
        return false;
    }

    if (avformat_open_input(&afmtCtx_, FileName.c_str(), nullptr, nullptr) < 0) {
        std::cout << "无法打开文件" << FileName << std::endl;
        return false;
    }
    if (avformat_find_stream_info(afmtCtx_, nullptr) < 0) {
        std::cout << "无法找到流信息" << std::endl;;
        return false;
    }

    videoStreamIndex_ = av_find_best_stream(fmtCtx_, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (videoStreamIndex_ < 0) {
        std::cout << "无法找到视频流" << std::endl;
        return false;
    }

    audioStreamIndex_ = av_find_best_stream(afmtCtx_, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
    if (audioStreamIndex_ < 0) {
        std::cout << "无法找到音频流" << std::endl;
    }

    AVStream* stream = fmtCtx_->streams[videoStreamIndex_];
    const AVCodec* codec = avcodec_find_decoder(stream->codecpar->codec_id);
    if (!codec) {
        std::cout << "无法找到解码器" << std::endl;
        return false;
    }

    if (audioStreamIndex_>=0)
    {
        AVStream* astream = fmtCtx_->streams[audioStreamIndex_];
        const AVCodec* acodec = avcodec_find_decoder(astream->codecpar->codec_id);
        if (!acodec) {
            std::cout << "无法找到解码器" << std::endl;
        }

        acodecCtx_ = avcodec_alloc_context3(acodec);
        avcodec_parameters_to_context(acodecCtx_, astream->codecpar);
        if (avcodec_open2(acodecCtx_, acodec, nullptr) < 0) {
            std::cout << "无法找到音频解码器" << std::endl;
        }
        atimeBase_ = av_q2d(astream->time_base);
        //输入声道布局
        AVChannelLayout inLayout{};
        if (acodecCtx_->ch_layout.nb_channels > 0) {
            av_channel_layout_copy(&inLayout, &acodecCtx_->ch_layout);;
        }
        else {
            av_channel_layout_default(&inLayout, 2);
        }

        //输入声道布局：立体声
        av_channel_layout_default(&outChLayout_, outChannels_);

        int ret = swr_alloc_set_opts2(
            &swrCtx_, &outChLayout_, outSampleFmt_, outSampleRate_,
            &inLayout, acodecCtx_ -> sample_fmt, acodecCtx_->sample_rate,
            0, nullptr);

        av_channel_layout_uninit(&inLayout);

        if (ret < 0 || swr_init(swrCtx_) < 0) {
            std::cout << "swr初始化失败" << std::endl;
            goto __fail;
        }

        apkt_ = av_packet_alloc();
        aframe_ = av_frame_alloc();

        std::cout << "音频采样率 = " << outSampleRate_ << "输出声道 = " <<
            outChannels_ << std::endl;
    }
    __fail:
    codecCtx_ = avcodec_alloc_context3(codec);
    avcodec_parameters_to_context(codecCtx_, stream->codecpar);
    if (avcodec_open2(codecCtx_, codec , nullptr) < 0) {
        std::cout << "无法找到解码器"<< std::endl;
        return false;
    }
    width_ = codecCtx_->width;
    height_ = codecCtx_->height;
    pixFmt_ = codecCtx_->pix_fmt;
    timeBase_ = av_q2d(stream->time_base);
    
    pkt_ = av_packet_alloc();
    rawframe_ = av_frame_alloc();

    // 
    if (pixFmt_ != AV_PIX_FMT_YUV420P) {
        swsCtx_=sws_getContext(width_,height_,pixFmt_,
                               width_,height_,AV_PIX_FMT_YUV420P,
                               SWS_BILINEAR, nullptr, nullptr, nullptr);

        convFrame_ = av_frame_alloc();
        convFrame_->format = AV_PIX_FMT_YUV420P;
        convFrame_->width = width_;
        convFrame_->height = height_;
        if (av_frame_get_buffer(convFrame_, 32) < 0) {
            std::cout << "无法为convFrame_分配缓存" << std::endl;
            return false;
        }
        std::cout << "使用swscale为视频帧格式转换为YUV420P" << pixFmt_ << std::endl;
    }
    std::cout << "width_" << 
        width_ << "height_"<< height_<< "pixFmt_"
        << pixFmt_ << std::endl;
    return true;
}
// 
bool Decoder::decodeNextFrame(AVFrame*& frameOut)
{
    while (true) {
        // 
        int ret = avcodec_receive_frame(codecCtx_, rawframe_);
        if (ret == 0) {
            frameOut = prepareFrame(rawframe_);
            return true;
        }
        if (ret == AVERROR_EOF)return false;
        if (ret != AVERROR(EAGAIN)) {
            std::cout << "receive_frame错误: " << ret << std::endl;
            return false;
        }
        // 
        {
            std::lock_guard<std::mutex> mtx(mtx_);
            ret = av_read_frame(fmtCtx_, pkt_);
        }
        if (ret < 0) {
            avcodec_send_packet(codecCtx_, nullptr);
            continue;
        }
        if (pkt_->stream_index == videoStreamIndex_) {
            avcodec_send_packet(codecCtx_, pkt_);
        }
        av_packet_unref(pkt_);
    }
    
    return true;
}

int Decoder::AudioNextFrame(std::vector<uint8_t>& out, double& pts)
{
    pts = 0.0;

    while (true) {
        int ret = avcodec_receive_frame(acodecCtx_, aframe_);
        if (ret == 0) {
            //当前帧pts有效则计算当前帧的时间，否则使用上一帧推算出的时间
            double aframePts = (aframe_->pts != AV_NOPTS_VALUE)
                ? aframe_->pts * atimeBase_ : nextPts_;
            //扩展vector的容量，用以存储重采样后的数据
            int nbSamples = aframe_->nb_samples;
            int outSize = nbSamples * outChannels_ * 2;//s16->2字节
            out.resize(outSize);

            uint8_t* outData[1] = { out.data() };//获取data的内存地址
            //写入数据
            int converted=swr_convert(swrCtx_,outData,nbSamples,
                (const uint8_t**)aframe_->extended_data, nbSamples);

            if (converted < 0) {
                std::cout << "重采样失败" << std::endl;
                return -1;
            }
            out.resize(converted * outChannels_ * 2);   

            nextPts_ = aframePts + (double)nbSamples / acodecCtx_->sample_rate;
            pts = aframePts;

            av_frame_unref(aframe_);
            return 1;
        }

        if (ret == AVERROR_EOF) {
            if (!eof_) {
                eof_ = true;

                int outSize = 4096 * outChannels_ * 2;
                out.resize(outSize);
                uint8_t* outData[1] = { out.data() };
                int converted = swr_convert(swrCtx_, outData, 4096, nullptr, 0);
                if (converted > 0) {
                    out.resize(converted * outChannels_ * 2);
                    return 1;
                }
            }
            return 0;
        }
        if (ret != AVERROR(EAGAIN)) {
            std::cout << "音频解码失败" << ret << std::endl;
            return -1;
        }
        // 需要更多数据，读新包
        int r = 0;
        {
            std::lock_guard<std::mutex> mtx(mtx_);
            r = av_read_frame(afmtCtx_, apkt_);
        }
        if (r < 0) {
            avcodec_send_packet(acodecCtx_, nullptr);
            continue;
        }
        if (apkt_->stream_index == audioStreamIndex_) {
            avcodec_send_packet(acodecCtx_, apkt_);
        }
        av_packet_unref(apkt_);

    }
}

AVFrame* Decoder::prepareFrame(AVFrame* in)
{
    if (!swsCtx_) return in;
    av_frame_make_writable(convFrame_);
    sws_scale(swsCtx_,
        in->data, in->linesize, 0, height_,
        convFrame_->data, convFrame_->linesize);

    return convFrame_;
}
