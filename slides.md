# `libcamera`

## Introduction

- Cameras have gone from highly complex black boxes to highly configurable
sensors
- Complexity has migrated up the stack to user \/ app space revealing the need for libcamera framework

## Camera Sensor applications

- Digital photo and video
- Home
- Industrial
- Intelligent agriculture and farming
- Medical and life sciences
- Smart Retail

---

### Camera Sensor Characteristics

- Resolution
- Frame rate
- Exposure time
- Gain - analogue, digital
- Color - monochrome, RGB
- Shutter - rolling, global
- Focus lenses and filters - Infrared, manual, auto
- Communication interface - MIPI CSI-2, I2C, SPI, USB

## V4L2/DVB architecture

- App communicates with camera using device files such as `/dev/video0` typically [using libv4l](https://linuxtv.org/downloads/v4l-dvb-apis/userspace-api/v4l/v4l2grab.c.html)
- Obtains image and video through camera driver such as `bcm2835_v4l2` that depends on camera sensor I2C driver, `videodev`, and other V4L kernel modules
- `v4l-utils` package provides commands such as `v4l2-ctl`

## `libcamera` architecture

- App communicates with camera through user space C++ framework library
- Raw data is obtained from camera driver such as `bcm2835_unicam`
- Camera sensor [I2C](https://en.wikipedia.org/wiki/I2C) driver such as `imx219` is used to send commands
- Data is processed using extensible image processing algorithms
- Supports legacy V4L apps and USB cameras
- [Camera Tuning](https://datasheets.raspberrypi.com/camera/raspberry-pi-camera-guide.pdf) is supported
- [Android Camera HAL3](https://source.android.com/devices/camera/camera3) implementation is available

## Image processing Algorithms

- Defective pixel correction (DPC)
- Spatial Denoise (SDN)
- Automatic White Balance (AWB)
- Automatic Exposure Control (AEC)
- Automatic Gain Control (AGC)
- Automatic Lens Shading Correction (ALSC)

## Camera Modules from `Raspberry Pi`

- Camera Module v1 with OmniVision OV5647
- Camera Module v2 with Sony IMX219
  - Filter-less Pi Noir version available
- HQ Camera with Sony IMX477

## Camera Modules from Arducam

- Camera modules for Raspberry Pi and other SoCs
- Monochrome Global Shutter with sensors such as OmniVision OV9281
- [64MP Autofocus 10x Digital Zoom Camera](https://www.youtube.com/watch?v=FZmmjNnrrbo) with Sony IMX686 sensor
- [Stereo/Quad-Scopic Cameras](https://www.arducam.com/raspberry-pi-multiple-cameras/)

## `libcamera-apps` from `Raspberry Pi`

```log
LIBCAMERA_LOG_LEVELS=*:DEBUG libcamera-hello --list-cameras

libcamera-jpeg -o test.jpg -t 2000 --shutter 20000 --gain 1.5

libcamera-still -r -o test.jpg
libcamera-still -o test.jpg --post-process-file drc.json

libcamera-vid -t 10000 -o test.h264 --save-pts timestamps.txt
mkvmerge -o test.mkv --timecodes 0:timestamps.txt test.h264

libcamera-vid -t 10000 --codec mjpeg -o test.mjpeg
```

## `libcamera` C++ API

- Open camera
- Configure streams
- Start camera
- Stop camera
- Close camera

---

### Open camera

Create instance of `libcamera::CameraManager` to acquire an instance of `libcamera::Camera`

```c++
camera_manager_ = std::make_unique<CameraManager>();
camera_manager_->start();
std::string const &cam_id = camera_manager_->cameras()[0]->id();
camera_ = camera_manager_->get(cam_id);
camera_->acquire();
```

---

### Configure streams

```c++
StreamRoles stream_roles = { StreamRole::VideoRecording };
// { StreamRole::StillCapture, StreamRole::Raw };
// { StreamRole::Viewfinder };
configuration_ = camera_->generateConfiguration(stream_roles);
configuration_->at(0).pixelFormat = libcamera::formats::YUV420;
configuration_->at(0).bufferCount = 2;
configuration_->at(0).size.width = 640;
configuration_->at(0).size.height = 480;
configuration_->validate();
camera_->configure(configuration_.get());
```

---

### Start camera

For each frame an application wants to capture it must queue a request for it to
the camera

```c++
Stream *stream = configuration_->at(0).stream();
allocator_ = new FrameBufferAllocator(camera_);
allocator_->allocate(stream);
const std::vector<std::unique_ptr<FrameBuffer>> &buffers =
    allocator_->buffers(stream);
camera_->start(&controls_);
controls_.clear();
camera_->requestCompleted.connect(requestComplete);
for (unsigned int i = 0; i < buffers.size(); ++i) {
    std::unique_ptr<Request> request = camera_->createRequest();
    request->addBuffer(stream, buffers[i].get());
    camera_->queueRequest(request.get());
    requests_.push_back(std::move(request));
}
```

---

### `requestComplete`

```c++
if (request->status() == Request::RequestCancelled)
    return;
const libcamera::Request::BufferMap &buffers = request->buffers();
for (auto bufferPair : buffers) {
    FrameBuffer *buffer = bufferPair.second;
    const FrameMetadata &metadata = buffer->metadata();
    std::cout << " seq: " << std::setw(6) << std::setfill('0')
              << metadata.sequence << std::endl;
    // Queue next request with same buffer
    std::unique_ptr<Request> requestToQueue = camera_->createRequest();
    const Stream *stream = bufferPair.first;
    requestToQueue->addBuffer(stream, buffer);
    camera_->queueRequest(requestToQueue.get());
    requests_.push_back(std::move(requestToQueue));
}

```

---

### Encoding image and video

- libcamera provides raw image frames from the sensor that are processed using image processing and control algorithms based on per sensor tuning file
- libcamera does not provide any means to encode or display images or videos
- `libcamera-still` uses external libraries to encode images such as libjpeg for jpeg
- `libcamera-vid` uses hardware H.264 encoder on Raspberry Pi through `/dev/video11`, apps may use external libraries such as ffmpeg/libav codec for H.264

---

### Stop camera

```c++
camera_->stop();
camera_->requestCompleted.disconnect(requestComplete);
requests_.clear();
```

---

### Close camera

- Configure streams, start and stop camera can repeat multiple times
- The application is now finished with the camera and the resources the camera
uses

```c++
  Stream *stream = configuration_->at(0).stream();
  allocator_->free(stream);
  delete allocator_;
  configuration_.reset();
  camera_->release();
  camera_manager_->stop();
```

---

### Build and run example

```log
$ c++ example.cpp -o example `pkg-config --cflags --libs libcamera` -std=c++17

$ ./example
[4:00:09.926918692] [3203]  INFO Camera camera_manager.cpp:293 libcamera v0.0.0+3545-22656360-dirty (2022-05-12T10:01:53-03:00)
[4:00:10.107994296] [3204]  WARN RPI raspberrypi.cpp:1241 Mismatch between Unicam and CamHelper for embedded data usage!
[4:00:10.110650760] [3204]  INFO RPI raspberrypi.cpp:1356 Registered camera /base/soc/i2c0mux/i2c@1/imx219@10 to Unicam device /dev/media0 and ISP device /dev/media3
[4:00:10.113007188] [3203]  INFO Camera camera.cpp:1029 configuring streams: (0) 640x480-YUV420
[4:00:10.114227143] [3204]  INFO RPI raspberrypi.cpp:760 Sensor: /base/soc/i2c0mux/i2c@1/imx219@10 - Selected sensor format: 640x480-SBGGR10_1X10 - Selected unicam format: 640x480-pBAA
Press enter to exit...
 seq: 000007
 seq: 000008
 seq: 000009
 seq: 000010
```

## Explore further

- [libcamera: The Future of Cameras on Linux is Now](https://www.youtube.com/watch?v=VNfq4L7mRtU)
- [libcamera Documentation](https://libcamera.org/docs.html)
- [libcamera API](https://libcamera.org/api-html)
- [Raspberry Pi Camera Guide](Camera)
- [Raspberry Pi Camera Algorithm and Tuning Guide](https://datasheets.raspberrypi.com/camera/raspberry-pi-camera-guide.pdf)