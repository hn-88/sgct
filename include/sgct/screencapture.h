/*****************************************************************************************
 * SGCT                                                                                  *
 * Simple Graphics Cluster Toolkit                                                       *
 *                                                                                       *
 * Copyright (c) 2012-2024                                                               *
 * For conditions of distribution and use, see copyright notice in LICENSE.md            *
 ****************************************************************************************/

#ifndef __SGCT__SCREENCAPTURE__H__
#define __SGCT__SCREENCAPTURE__H__

#include <sgct/sgctexports.h>
#include <sgct/math.h>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace sgct {

class Image;

/**
 * This class is used internally by SGCT and is called when taking screenshots.
 */
class SGCT_EXPORT ScreenCapture {
public:
    /**
     * The different file formats supported.
     */
    enum class CaptureFormat { PNG, TGA, JPEG };
    enum class CaptureSource { Texture, BackBuffer, LeftBackBuffer, RightBackBuffer };
    enum class EyeIndex { Mono, StereoLeft, StereoRight };

    struct ScreenCaptureThreadInfo {
        std::string filename;
        std::unique_ptr<Image> frameBufferImage;
        std::unique_ptr<std::thread> captureThread;
        std::mutex* mutex = nullptr;
        bool isRunning = false; // needed for test if running without join
    };

    ScreenCapture(const Window& window, ScreenCapture::EyeIndex ei, int bytesPerColor,
        unsigned int colorDataType);
    ~ScreenCapture();

    /**
     * Initializes the PBO or re-sizes it if the frame buffer size have changed.
     *
     * \param resolution The pixel resolution of the frame buffer
     */
    void resize(ivec2 resolution);

    /**
     * This function saves the images to disc.
     *
     * \param textureId The texture that will be streamed from the GPU if frame buffer
     *        objects are used in the rendering
     * \param capSrc The object that should be captured
     */
    void saveScreenCapture(unsigned int textureId,
        CaptureSource capSrc = CaptureSource::Texture);

private:
    std::string createFilename(uint64_t frameNumber);
    int availableCaptureThread();
    Image* prepareImage(int index, std::string file);

    std::mutex _mutex;
    std::vector<ScreenCaptureThreadInfo> _captureInfos;

    unsigned int _nThreads;
    unsigned int _pbo = 0;
    unsigned int _downloadType = 0x1401; // GL_UNSIGNED_BYTE;
    int _dataSize = 0;
    ivec2 _resolution = ivec2{ 0, 0 };
    int _bytesPerColor = 1;

    const EyeIndex _eyeIndex;
    const Window& _window;
};

} // namespace sgct

#endif // __SGCT__SCREENCAPTURE__H__
