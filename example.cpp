#include <libcamera/libcamera.h>
#include <iomanip>
#include <iostream>

using namespace libcamera;

std::unique_ptr<CameraManager> camera_manager_;
std::shared_ptr<Camera> camera_;
std::unique_ptr<CameraConfiguration> configuration_;
FrameBufferAllocator *allocator_ = nullptr;
std::vector<std::unique_ptr<Request>> requests_;
ControlList controls_;

void OpenCamera()
{
    camera_manager_ = std::make_unique<CameraManager>();
    camera_manager_->start();
    std::string const &cam_id = camera_manager_->cameras()[0]->id();
    camera_ = camera_manager_->get(cam_id);
    camera_->acquire();
}

void ConfigureStreams()
{
    StreamRoles stream_roles = { StreamRole::VideoRecording };
    configuration_ = camera_->generateConfiguration(stream_roles);
    configuration_->at(0).pixelFormat = libcamera::formats::YUV420;
    configuration_->at(0).bufferCount = 2;
    configuration_->at(0).size.width = 640;
    configuration_->at(0).size.height = 480;
    configuration_->validate();
    camera_->configure(configuration_.get());
}

static void requestComplete(Request *request)
{
    if (request->status() == Request::RequestCancelled)
        return;
    const libcamera::Request::BufferMap &buffers = request->buffers();
    for (auto bufferPair : buffers) {
        FrameBuffer *buffer = bufferPair.second;
        const FrameMetadata &metadata = buffer->metadata();
        std::cout << " seq: " << std::setw(6) << std::setfill('0') << metadata.sequence << std::endl;
        // Queue next request with same buffer
        std::unique_ptr<Request> requestToQueue = camera_->createRequest();
        const Stream *stream = bufferPair.first;
        requestToQueue->addBuffer(stream, buffer);
        camera_->queueRequest(requestToQueue.get());
        requests_.push_back(std::move(requestToQueue));
    }
}

void StartCamera()
{
    Stream *stream = configuration_->at(0).stream();
    allocator_ = new FrameBufferAllocator(camera_);
    allocator_->allocate(stream);
    const std::vector<std::unique_ptr<FrameBuffer>> &buffers = allocator_->buffers(stream);
    camera_->start(&controls_);
    controls_.clear();
    camera_->requestCompleted.connect(requestComplete);
    for (unsigned int i = 0; i < buffers.size(); ++i) {
        std::unique_ptr<Request> request = camera_->createRequest();
        request->addBuffer(stream, buffers[i].get());
        camera_->queueRequest(request.get());
        requests_.push_back(std::move(request));
    }
}

void StopCamera()
{
    camera_->stop();
    camera_->requestCompleted.disconnect(requestComplete);
    requests_.clear();
}

void CloseCamera()
{
    Stream *stream = configuration_->at(0).stream();
    allocator_->free(stream);
    delete allocator_;
    configuration_.reset();
    camera_->release();
    camera_manager_->stop();
}

int main()
{
    OpenCamera();
    ConfigureStreams();
    StartCamera();

    std::cout << "Press enter to exit..." << std::endl;
    std::cin.ignore();

    StopCamera();
    CloseCamera();    
    return 0;
}
