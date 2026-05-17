#include "recording/frame_ring_buffer.h"

#include <iostream>

FrameRingBuffer::FrameRingBuffer(std::size_t max_frames)
    : max_frames_(max_frames),
      frames_(max_frames) {}

void FrameRingBuffer::push(const cv::Mat& frame) {
    if (frame.empty() || max_frames_ == 0) {
        return;
    }

    std::lock_guard<std::mutex> lock(mutex_);

    frames_[write_index_] = frame.clone();
    write_index_ = (write_index_ + 1) % max_frames_;

    if (write_index_ == 0) {
        filled_ = true;
    }
}

std::vector<cv::Mat> FrameRingBuffer::snapshot() const {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<cv::Mat> result;

    const std::size_t count = filled_ ? max_frames_ : write_index_;
    result.reserve(count);

    if (!filled_) {
        for (std::size_t i = 0; i < write_index_; ++i) {
            if (!frames_[i].empty()) {
                result.push_back(frames_[i].clone());
            }
        }
        return result;
    }

    for (std::size_t i = 0; i < max_frames_; ++i) {
        const std::size_t idx = (write_index_ + i) % max_frames_;
        if (!frames_[idx].empty()) {
            result.push_back(frames_[idx].clone());
        }
    }

    return result;
}

bool FrameRingBuffer::saveToVideo(const std::string& output_path,
                                  double output_fps) const {
    std::vector<cv::Mat> frames = snapshot();

    if (frames.empty()) {
        std::cerr << "[FrameRingBuffer] No frames to save.\n";
        return false;
    }

    const cv::Size frame_size(frames[0].cols, frames[0].rows);

    cv::VideoWriter writer(
        output_path,
        cv::VideoWriter::fourcc('M', 'J', 'P', 'G'),
        output_fps,
        frame_size
    );

    if (!writer.isOpened()) {
        std::cerr << "[FrameRingBuffer] Failed to open writer: "
                  << output_path << "\n";
        return false;
    }

    for (const auto& frame : frames) {
        if (frame.empty()) {
            continue;
        }

        if (frame.size() == frame_size) {
            writer.write(frame);
        } else {
            cv::Mat resized;
            cv::resize(frame, resized, frame_size);
            writer.write(resized);
        }
    }

    writer.release();

    std::cout << "[FrameRingBuffer] Saved clip: " << output_path << "\n";
    return true;
}

std::size_t FrameRingBuffer::size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return filled_ ? max_frames_ : write_index_;
}