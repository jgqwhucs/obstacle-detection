//
// Created by paul on 12/05/15.
//

#include "Image8bit.h"

Image8bit Image8bit::fromMat(cv::Mat src) {
    Image8bit image = Image8bit(src.cols, src.rows, src.type());
    for(int x = 0; x < src.cols; x++) {
        for(int y=0; y < src.rows; y++) {
            image.at<uint8_t>(x, y) = src.at<uint8_t>(x, y);
        }
    }
    return image;
}
