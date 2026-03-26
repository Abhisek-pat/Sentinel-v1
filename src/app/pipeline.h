/// \brief This is the main orchestration class

class Pipeline {
public:
    explicit Pipeline(const std::string& source_path);
    bool initialize();
    void run();

private:
    std::string source_path_;
};