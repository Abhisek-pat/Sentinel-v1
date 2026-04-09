class YoloOnnxDetector {
public:
    YoloOnnxDetector(const std::string& model_path,
                     const std::vector<std::string>& class_names,
                     float conf_threshold = 0.4f,
                     float iou_threshold = 0.5f);

    bool initialize();
    DetectionResult detect(const cv::Mat& frame);

private:
    std::string model_path_;
    std::vector<std::string> class_names_;
    float conf_threshold_;
    float iou_threshold_;
};