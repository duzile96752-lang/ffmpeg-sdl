#include "FrameQueue.h"
//生产者调用
bool FrameQueue::push(AVFrame* frame)
{
    std::unique_lock<std::mutex> lock(mtx_);
    cvNotFull_.wait(lock, [this] {
        // 队列满了就阻塞，等到消费者取走再 push
        return aborted_ || q_.size() < maxSize_;
        });
    if (aborted_)return false;

    q_.push(frame);
    cvNotEmpty_.notify_one();// 唤醒消费者

    return true;
}
//消费者调用
AVFrame* FrameQueue::pop()
{
    std::unique_lock<std::mutex> lock(mtx_);
    cvNotEmpty_.wait(lock, [this] {
        return aborted_ || !q_.empty();
        });
    if (q_.empty())return nullptr;

    AVFrame* f = q_.front();
    q_.pop();
    cvNotFull_.notify_one();// 唤醒生产者

    return f;
}
// 强制中止：清空队列、唤醒所有等待者
void FrameQueue::abort()
{
    std::lock_guard<std::mutex> lock(mtx_);
    aborted_ = true;
    while (!q_.empty()) {
        AVFrame* f = q_.front();
        q_.pop();
        if (f)av_frame_free(&f);
    }
    cvNotFull_.notify_all();
    cvNotEmpty_.notify_all();
}

bool FrameQueue::isAborted() const
{
    std::lock_guard<std::mutex> lock(mtx_);
    return aborted_;
}
