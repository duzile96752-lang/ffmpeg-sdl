// ============================================================
// FFmpeg + OpenGL (GLEW) 多线程视频播放器
// 线程模型：1 解码线程 + 1 渲染线程（主线程）
// 同步机制：std::mutex + std::condition_variable + 有界队列
// ============================================================

#include <GL/glew.h>
#include <GLFW/glfw3.h>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libavutil/time.h>
#include <libswscale/swscale.h>
}

#include <cstdio>
#include <cstring>
#include <string>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <atomic>
#include <chrono>

// ============================================================
// 着色器（与单线程版一致）
// ============================================================
static const char* VERT_SRC = R"(
#version 330 core
layout(location = 0) in vec2 a_pos;
layout(location = 1) in vec2 a_uv;
out vec2 v_uv;
void main() {
    v_uv = a_uv;
    gl_Position = vec4(a_pos, 0.0, 1.0);
}
)";

static const char* FRAG_SRC = R"(
#version 330 core
in vec2 v_uv;
out vec4 FragColor;
uniform sampler2D texY;
uniform sampler2D texU;
uniform sampler2D texV;

void main() {
    float y = texture(texY, v_uv).r;
    float u = texture(texU, v_uv).r - 0.5;
    float v = texture(texV, v_uv).r - 0.5;
    y = (y - 16.0/255.0) * (255.0/219.0);
    float r = y + 1.402 * v;
    float g = y - 0.344 * u - 0.714 * v;
    float b = y + 1.772 * u;
    FragColor = vec4(clamp(r,0.0,1.0),
                     clamp(g,0.0,1.0),
                     clamp(b,0.0,1.0), 1.0);
}
)";

// ============================================================
// FFmpegDecoder（与单线程版一致，仅在解码线程使用）
// ============================================================
class FFmpegDecoder {
public:
    bool open(const std::string& filename) {
        if (avformat_open_input(&fmtCtx_, filename.c_str(), nullptr, nullptr) < 0) {
            fprintf(stderr, "[FFmpeg] 无法打开文件: %s\n", filename.c_str());
            return false;
        }
        if (avformat_find_stream_info(fmtCtx_, nullptr) < 0) {
            fprintf(stderr, "[FFmpeg] 无法找到流信息\n");
            return false;
        }

        videoStreamIndex_ = av_find_best_stream(
            fmtCtx_, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
        if (videoStreamIndex_ < 0) {
            fprintf(stderr, "[FFmpeg] 未找到视频流\n");
            return false;
        }

        AVStream* stream = fmtCtx_->streams[videoStreamIndex_];
        const AVCodec* codec = avcodec_find_decoder(stream->codecpar->codec_id);
        if (!codec) {
            fprintf(stderr, "[FFmpeg] 找不到解码器\n");
            return false;
        }

        codecCtx_ = avcodec_alloc_context3(codec);
        avcodec_parameters_to_context(codecCtx_, stream->codecpar);
        if (avcodec_open2(codecCtx_, codec, nullptr) < 0) {
            fprintf(stderr, "[FFmpeg] 打不开解码器\n");
            return false;
        }

        width_ = codecCtx_->width;
        height_ = codecCtx_->height;
        pixFmt_ = codecCtx_->pix_fmt;
        timeBase_ = av_q2d(stream->time_base);

        packet_ = av_packet_alloc();
        rawFrame_ = av_frame_alloc();

        if (pixFmt_ != AV_PIX_FMT_YUV420P) {
            swsCtx_ = sws_getContext(
                width_, height_, pixFmt_,
                width_, height_, AV_PIX_FMT_YUV420P,
                SWS_BILINEAR, nullptr, nullptr, nullptr);

            convFrame_ = av_frame_alloc();
            convFrame_->format = AV_PIX_FMT_YUV420P;
            convFrame_->width = width_;
            convFrame_->height = height_;
            if (av_frame_get_buffer(convFrame_, 32) < 0) {
                fprintf(stderr, "[FFmpeg] 无法分配转换帧缓冲区\n");
                return false;
            }
            printf("[FFmpeg] 源格式 %d → 用 swscale 转为 YUV420P\n", pixFmt_);
        }

        printf("[FFmpeg] 已打开: %dx%d, 像素格式=%d\n",
            width_, height_, pixFmt_);
        return true;
    }

    ~FFmpegDecoder() {
        if (convFrame_)  av_frame_free(&convFrame_);
        if (rawFrame_)   av_frame_free(&rawFrame_);
        if (packet_)     av_packet_free(&packet_);
        if (codecCtx_)   avcodec_free_context(&codecCtx_);
        if (fmtCtx_)     avformat_close_input(&fmtCtx_);
        if (swsCtx_)     sws_freeContext(swsCtx_);
    }

    // 注意：只允许在解码线程调用（非线程安全）
    bool decodeNextFrame(AVFrame*& frameOut) {
        while (true) {
            int ret = avcodec_receive_frame(codecCtx_, rawFrame_);
            if (ret == 0) {
                frameOut = prepareFrame(rawFrame_);
                return true;
            }
            if (ret == AVERROR_EOF) return false;
            if (ret != AVERROR(EAGAIN)) {
                fprintf(stderr, "[FFmpeg] receive_frame 错误\n");
                return false;
            }

            ret = av_read_frame(fmtCtx_, packet_);
            if (ret < 0) {
                avcodec_send_packet(codecCtx_, nullptr);
                continue;
            }
            if (packet_->stream_index == videoStreamIndex_) {
                avcodec_send_packet(codecCtx_, packet_);
            }
            av_packet_unref(packet_);
        }
    }

    int width()  const { return width_; }
    int height() const { return height_; }
    double timeBase() const { return timeBase_; }

private:
    AVFrame* prepareFrame(AVFrame* in) {
        if (!swsCtx_) return in;
        av_frame_make_writable(convFrame_);
        sws_scale(swsCtx_,
            in->data, in->linesize, 0, height_,
            convFrame_->data, convFrame_->linesize);
        return convFrame_;
    }

    AVFormatContext* fmtCtx_ = nullptr;
    AVCodecContext* codecCtx_ = nullptr;
    AVPacket* packet_ = nullptr;
    AVFrame* rawFrame_ = nullptr;
    AVFrame* convFrame_ = nullptr;
    SwsContext* swsCtx_ = nullptr;

    int videoStreamIndex_ = -1;
    int width_ = 0, height_ = 0;
    AVPixelFormat pixFmt_ = AV_PIX_FMT_NONE;
    double timeBase_ = 0.0;
};

// ============================================================
// ★ 新增：线程安全的有界帧队列
//    - 生产者（解码线程）：队列满时阻塞，等到消费者取走再 push
//    - 消费者（渲染线程）：队列空时阻塞，等到生产者 push 再取
//    - abort()：强制唤醒两端，用于退出
// ============================================================
class FrameQueue {
public:
    explicit FrameQueue(size_t maxSize) : maxSize_(maxSize) {}

    // 生产者调用。返回 false 表示队列已 abort，调用方需自行释放 frame
    bool push(AVFrame* frame) {
        std::unique_lock<std::mutex> lock(mtx_);
        cvNotFull_.wait(lock, [this] {
            return aborted_ || q_.size() < maxSize_;
            });
        if (aborted_) return false;

        q_.push(frame);
        cvNotEmpty_.notify_one();
        return true;
    }

    // 消费者调用。返回 nullptr 表示流结束或队列被 abort
    AVFrame* pop() {
        std::unique_lock<std::mutex> lock(mtx_);
        cvNotEmpty_.wait(lock, [this] {
            return aborted_ || !q_.empty();
            });

        // abort 且队列空 → 结束
        if (q_.empty()) return nullptr;

        AVFrame* f = q_.front();
        q_.pop();
        cvNotFull_.notify_one();
        return f;
    }

    // 强制中止：清空队列、唤醒所有等待者
    void abort() {
        std::lock_guard<std::mutex> lock(mtx_);
        aborted_ = true;
        while (!q_.empty()) {
            AVFrame* f = q_.front();
            q_.pop();
            if (f) av_frame_free(&f);
        }
        cvNotFull_.notify_all();
        cvNotEmpty_.notify_all();
    }

    bool isAborted() const {
        std::lock_guard<std::mutex> lock(mtx_);
        return aborted_;
    }

private:
    std::queue<AVFrame*>       q_;
    mutable std::mutex         mtx_;         // 保护 q_ 和 aborted_
    std::condition_variable    cvNotFull_;   // "队列没满" 条件
    std::condition_variable    cvNotEmpty_;  // "队列非空" 条件
    size_t                     maxSize_;
    bool                       aborted_ = false;
};

// ============================================================
// ★ 解码线程函数
// ============================================================
static void decodeThreadFunc(FFmpegDecoder* decoder, FrameQueue* queue) {
    printf("[Decoder] 解码线程启动\n");
    int frameCount = 0;

    AVFrame* frame = nullptr;
    while (!queue->isAborted()) {
        if (!decoder->decodeNextFrame(frame)) {
            printf("[Decoder] 解码结束，共 %d 帧\n", frameCount);
            queue->push(nullptr);  // EOF 哨兵：告诉渲染线程该结束了
            break;
        }

        // clone 一份：因为 decoder 内部的 rawFrame_/convFrame_
        // 会在下一帧解码时被复用，必须深拷贝一份出来
        AVFrame* cloned = av_frame_clone(frame);
        if (!cloned) {
            fprintf(stderr, "[Decoder] av_frame_clone 失败\n");
            queue->push(nullptr);
            break;
        }

        // 塞进队列（满了会在这里阻塞）
        if (!queue->push(cloned)) {
            av_frame_free(&cloned);  // 队列已 abort，释放
            break;
        }
        ++frameCount;
    }

    printf("[Decoder] 解码线程退出\n");
}

// ============================================================
// GLRenderer（与单线程版一致）
// ============================================================
class GLRenderer {
public:
    bool init(int videoWidth, int videoHeight) {
        videoWidth_ = videoWidth;
        videoHeight_ = videoHeight;

        program_ = createProgram(VERT_SRC, FRAG_SRC);
        if (!program_) return false;

        float verts[] = {
            -1.0f, -1.0f, 0.0f, 1.0f,
             1.0f, -1.0f, 1.0f, 1.0f,
            -1.0f,  1.0f, 0.0f, 0.0f,
             1.0f,  1.0f, 1.0f, 0.0f,
        };
        glGenVertexArrays(1, &vao_);
        glGenBuffers(1, &vbo_);
        glBindVertexArray(vao_);
        glBindBuffer(GL_ARRAY_BUFFER, vbo_);
        glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE,
            4 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE,
            4 * sizeof(float), (void*)(2 * sizeof(float)));
        glBindVertexArray(0);

        glGenTextures(1, &texY_);
        glGenTextures(1, &texU_);
        glGenTextures(1, &texV_);
        setupTexParams(texY_);
        setupTexParams(texU_);
        setupTexParams(texV_);

        glUseProgram(program_);
        glUniform1i(glGetUniformLocation(program_, "texY"), 0);
        glUniform1i(glGetUniformLocation(program_, "texU"), 1);
        glUniform1i(glGetUniformLocation(program_, "texV"), 2);
        return true;
    }

    void renderFrame(AVFrame* frame) {
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        glUseProgram(program_);
        glBindVertexArray(vao_);

        int chromaW = videoWidth_ / 2;
        int chromaH = videoHeight_ / 2;

        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texY_);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, frame->linesize[0]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R8,
            videoWidth_, videoHeight_, 0,
            GL_RED, GL_UNSIGNED_BYTE, frame->data[0]);

        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, texU_);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, frame->linesize[1]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R8,
            chromaW, chromaH, 0,
            GL_RED, GL_UNSIGNED_BYTE, frame->data[1]);

        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, texV_);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, frame->linesize[2]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R8,
            chromaW, chromaH, 0,
            GL_RED, GL_UNSIGNED_BYTE, frame->data[2]);

        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);

        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        glBindVertexArray(0);
    }

private:
    void setupTexParams(GLuint tex) {
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, 
            GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, 
            GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, 
            GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, 
            GL_CLAMP_TO_EDGE);
    }

    GLuint compileShader(GLenum type, const char* src) {
        GLuint s = glCreateShader(type);
        glShaderSource(s, 1, &src, nullptr);
        glCompileShader(s);
        GLint ok = 0;
        glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
        if (!ok) {
            char log[1024];
            glGetShaderInfoLog(s, sizeof(log), nullptr, log);
            fprintf(stderr, "Shader 编译失败: %s\n", log);
            return 0;
        }
        return s;
    }

    GLuint createProgram(const char* vs, const char* fs) {
        GLuint v = compileShader(GL_VERTEX_SHADER, vs);
        GLuint f = compileShader(GL_FRAGMENT_SHADER, fs);
        if (!v || !f) return 0;
        GLuint p = glCreateProgram();
        glAttachShader(p, v);
        glAttachShader(p, f);
        glLinkProgram(p);
        GLint ok = 0;
        glGetProgramiv(p, GL_LINK_STATUS, &ok);
        if (!ok) {
            char log[1024];
            glGetProgramInfoLog(p, sizeof(log), nullptr, log);
            fprintf(stderr, "Program 链接失败: %s\n", log);
        }
        glDeleteShader(v);
        glDeleteShader(f);
        return p;
    }

    GLuint program_ = 0, vao_ = 0, vbo_ = 0;
    GLuint texY_ = 0, texU_ = 0, texV_ = 0;
    int videoWidth_ = 0, videoHeight_ = 0;
};

// ============================================================
// main
// ============================================================
int main(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "用法: %s <视频文件>\n", argv[0]);
        return 1;
    }

    // 1) 打开解码器
    FFmpegDecoder decoder;
    if (!decoder.open(argv[1])) return 1;
    int    W = decoder.width();
    int    H = decoder.height();
    double timeBase = decoder.timeBase();  // 提前取出，避免跨线程访问

    // 2) 创建帧队列并启动解码线程
    FrameQueue frameQueue(8);   // 最多缓存 8 帧，防止内存暴涨
    std::thread decodeThread(decodeThreadFunc, &decoder, &frameQueue);

    // 3) GLFW 窗口
    if (!glfwInit()) {
        fprintf(stderr, "GLFW 初始化失败\n");
        frameQueue.abort();
        decodeThread.join();
        return 1;
    }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    GLFWwindow* window = glfwCreateWindow(
        W, H, "FFmpeg + OpenGL 多线程播放器", nullptr, nullptr);
    if (!window) {
        fprintf(stderr, "创建窗口失败\n");
        glfwTerminate();
        frameQueue.abort();
        decodeThread.join();
        return 1;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    GLenum err = glewInit();
    if (err != GLEW_OK) {
        fprintf(stderr, "GLEW 初始化失败: %s\n", glewGetErrorString(err));
        glfwDestroyWindow(window);
        glfwTerminate();
        frameQueue.abort();
        decodeThread.join();
        return 1;
    }
    printf("[GL] %s | GLEW %s\n",
        glGetString(GL_VERSION), glewGetString(GLEW_VERSION));

    GLRenderer renderer;
    if (!renderer.init(W, H)) {
        glfwDestroyWindow(window);
        glfwTerminate();
        frameQueue.abort();
        decodeThread.join();
        return 1;
    }

    // 4) 渲染主循环（消费者）
    AVFrame* frame = nullptr;
    double   firstPts = -1.0;
    auto     wallStart = std::chrono::steady_clock::now();

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        // ★ 从队列取一帧（队列空时会阻塞在这一行，
        //   直到解码线程送帧过来）
        frame = frameQueue.pop();
        if (frame == nullptr) {
            printf("[Player] 视频播放结束\n");
            break;
        }

        // ---- 时间同步 ----
        double ptsSec = frame->pts * timeBase;
        if (firstPts < 0) {
            firstPts = ptsSec;
            wallStart = std::chrono::steady_clock::now();
        }
        double targetSec = ptsSec - firstPts;
        double elapsedSec = std::chrono::duration<double>(
            std::chrono::steady_clock::now() - wallStart).count();
        if (elapsedSec < targetSec) {
            std::this_thread::sleep_for(
                std::chrono::duration<double>(targetSec - elapsedSec));
        }

        // ---- 视口保持纵横比 ----
        int fbW, fbH;
        glfwGetFramebufferSize(window, &fbW, &fbH);
        float windowRatio = (float)fbW / (float)fbH;
        float videoRatio = (float)W / (float)H;
        int vpX = 0, vpY = 0, vpW = fbW, vpH = fbH;
        if (windowRatio > videoRatio) {
            vpW = (int)(fbH * videoRatio);
            vpX = (fbW - vpW) / 2;
        }
        else {
            vpH = (int)(fbW / videoRatio);
            vpY = (fbH - vpH) / 2;
        }
        glViewport(vpX, vpY, vpW, vpH);

        // ---- 渲染 ----
        renderer.renderFrame(frame);

        // ★ 用完立刻释放，让 GPU 内存/引用尽早归还
        av_frame_free(&frame);

        glfwSwapBuffers(window);
    }

    // 5) 优雅退出：唤醒可能阻塞的解码线程，然后 join
    printf("[Player] 正在退出...\n");
    frameQueue.abort();
    if (decodeThread.joinable()) {
        decodeThread.join();
    }

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}