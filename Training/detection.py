from ultralytics import YOLO

def main() -> None:
    model = YOLO("yolov8n.pt")
    model.export(format="onnx")

if __name__ == "__main__":
    main()