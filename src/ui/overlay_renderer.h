class OverlayRenderer {
public:
    void drawDetections(cv::Mat& frame, const std::vector<Detection>& detections);
    void drawStats(cv::Mat& frame, double fps, double inference_ms);
};