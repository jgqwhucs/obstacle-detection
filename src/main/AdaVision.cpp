#include <iostream>
#include <imageProc/dlib/DLibProcessor.h>
#include <io/cameradata/CameraDataHandler.h>
#include <opencv2/highgui/highgui.hpp>
#include <imageProc/liveFeed/base64EncDec.h>
#include <io/JSONSerializer.h>
#include <comm/ImageFeedZmq.h>
#include <imageProc/liveFeed/Compressor.h>
#include <io/cameradata/CameraDataProcessor.h>
#include <io/FileSystemImageStream.h>
#include <io/cameradata/ImageStreamCameraDataAdapter.h>
#include <io/cameradata/CameraDataNetworkStream.h>
#include <QtCore/QTS>
#include <dlib/config_reader.h>


class AdaVisionHandler : public CameraDataHandler {

public:

    AdaVisionHandler(std::string outputDir, const int zmqPort, const bool debug)
            : CameraDataHandler(), _zmqfeed(ZmqContextSingleton::getContext()),
              _zmqPort(zmqPort), _outputDir(outputDir), _debug(debug) {

        _zmqfeed.init(zmqPort);
        if (!QDir(outputDir.c_str()).exists()) {
            QDir().mkdir(outputDir.c_str());
        }
    }

    void onImageProcessed(std::vector<CameraData> cameraData,
                          std::vector<dlib::rectangle> detectedRectangles) {

        //TODO add imu logging
        if (cameraData.size() == 1) {
            onSingleImageProcessed(cameraData[0], detectedRectangles);
        } else {
            onMultiImageProcessed(cameraData, detectedRectangles);
        }

    }

    void onMultiImageProcessed(const vector<CameraData> &cameraData,
                               const std::vector<dlib::rectangle> detectedRectangles) {
        if (_debug) {
            std::cout << "received two images" << std::endl;
        }

        frame_counter++;
        std::ostringstream leftImageName;
        leftImageName << _outputDir << "/img_" << frame_counter << "_L" << ".png";
        imwrite(leftImageName.str(), cameraData[0].frame);

        std::ostringstream rightImageName;
        rightImageName << _outputDir << "/img_" << frame_counter << "_R" << ".png";
        imwrite(rightImageName.str(), cameraData[1].frame);

        //TODO something smarter than this...
        int cameraToUse = frame_counter % 2;
        std::vector<uchar> buff = Compressor::imgToBuff(cameraData[cameraToUse].frame, 3);
        sendProcessedImage(detectedRectangles, buff);
    }

    void onSingleImageProcessed(CameraData cameraData, std::vector<dlib::rectangle> detectedRectangles) {
        if (_debug) {
            std::cout << "single received image" << std::endl;
        }

        frame_counter++;
        std::ostringstream stringStream;
        stringStream << _outputDir << "/img_" << frame_counter << ".png";
        cv::imwrite(stringStream.str(), cameraData.frame);

        std::vector<uchar> buff = Compressor::imgToBuff(cameraData.frame, 3);
        sendProcessedImage(detectedRectangles, buff);
    }

    void sendProcessedImage(const vector<dlib::rectangle> &detectedRectangles, vector<uchar> &buff) {
        std::string encoded = base64_encode(buff.data(), buff.size());
        unique_ptr<std::string> JSON(new std::string(makeJSON(encoded, detectedRectangles, IMAGE16BIT)));
        _zmqfeed.sendFrame((const uint8_t *) JSON->c_str(), JSON->size());
    }

private:
    int frame_counter = 0;

    std::string _outputDir;

    ImageFeedZmq _zmqfeed;

    int _zmqPort;

    bool _debug;

};


void printUsage(int argc, char **argv) {
    std::cout << "Usage:\n"
            "adaVision config.txt" << std::endl;
    std::cout << "You entered: " << std::endl;
    for (int i = 0; i < argc; i++) {
        std::cout << argv[i];
    }
    std::cout << std::endl;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        printUsage(argc, argv);
        return 1;
    }
    dlib::config_reader cr(argv[1]);

    dlib::object_detector<DLibProcessor::image_scanner_type> detector;
    std::vector<dlib::object_detector<DLibProcessor::image_scanner_type>> detectors;

    const dlib::config_reader &models = cr.block("models");
    std::vector<std::string> keys;
    models.get_keys(keys);

    for (int i = 0; i < keys.size(); i++) {
        dlib::deserialize(models[keys[i]]) >> detector;
        detectors.push_back(detector);
    }
    DLibProcessor dLibProcessor(detectors);

    bool debug = dlib::get_option(cr, "debug", false);

    if (cr["mode"] == "file") {

        CameraDataProcessor cameraDataProcessor(
                new ImageStreamCameraDataAdapter(new FileSystemImageStream(cr["input_dir"], "*.png"),
                                                 dlib::get_option(cr, "double_up", false)),
                NULL,
                &dLibProcessor,
                new AdaVisionHandler(cr["output_dir"],
                                     dlib::get_option(cr, "zmq_out", 5555),
                                     dlib::get_option(cr, "debug", false)));
        try {
            cameraDataProcessor.run();
        } catch (std::exception &e) {
            std::cout << e.what() << endl;
            //This is to give time for zeromq to finish sending when reading from file system
            sleep(5);
        }

    } else if (cr["mode"] == "network") {
        CameraDataProcessor cameraDataProcessor(
                new CameraDataNetworkStream(cr["ip_address"], cr["port"]),
                NULL,
                &dLibProcessor,
                new AdaVisionHandler(cr["output_dir"],
                                     dlib::get_option(cr, "zmq_out", 5555),
                                     dlib::get_option(cr, "debug", false)));

        try {
            cameraDataProcessor.run();
        } catch (exception &e) {
            std::cout << e.what() << endl;
        }

    } else {
        printUsage(argc, argv);
    }
}

