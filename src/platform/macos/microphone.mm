/**
 * @file src/platform/macos/microphone.mm
 * @brief Definitions for microphone capture on macOS.
 */
#include "src/platform/common.h"
#include "src/platform/macos/av_audio.h"

#include <atomic>

#include "src/config.h"
#include "src/logging.h"

namespace platf {
  using namespace std::literals;

  bool
  request_microphone_permission(AVAuthorizationStatus authorization_status, const microphone_permission_request_t &request_access) {
    if (authorization_status == AVAuthorizationStatusNotDetermined) {
      BOOST_LOG(info) << "Requesting microphone access."sv;
    }

    auto permission_granted = std::atomic<bool> { authorization_status == AVAuthorizationStatusAuthorized };
    if (authorization_status == AVAuthorizationStatusNotDetermined) {
      auto permission_resolved = dispatch_semaphore_create(0);
      request_access([&](bool granted) {
        permission_granted.store(granted, std::memory_order_release);
        dispatch_semaphore_signal(permission_resolved);
      });

      dispatch_semaphore_wait(permission_resolved, DISPATCH_TIME_FOREVER);
      dispatch_release(permission_resolved);
    }

    if (!permission_granted.load(std::memory_order_acquire)) {
      if (authorization_status == AVAuthorizationStatusRestricted) {
        BOOST_LOG(error) << "Microphone access is restricted by macOS."sv;
      }
      else {
        BOOST_LOG(error) << "Microphone access was denied. Enable Foundation Sunshine in System Settings -> Privacy & Security -> Microphone."sv;
      }
    }

    return permission_granted.load(std::memory_order_acquire);
  }

  bool
  request_microphone_permission() {
    return request_microphone_permission(
      [AVCaptureDevice authorizationStatusForMediaType:AVMediaTypeAudio],
      [](microphone_permission_callback_t callback) {
        [AVCaptureDevice requestAccessForMediaType:AVMediaTypeAudio
                                completionHandler:^(BOOL granted) {
                                  callback(granted == YES);
                                }];
      }
    );
  }

  struct av_mic_t: public mic_t {
    AVAudio *av_audio_capture {};

    ~av_mic_t() override {
      [av_audio_capture release];
    }

    capture_e
    sample(std::vector<float> &sample_in) override {
      auto sample_size = sample_in.size();

      uint32_t length = 0;
      void *byteSampleBuffer = TPCircularBufferTail(&av_audio_capture->audioSampleBuffer, &length);

      while (length < sample_size * sizeof(float)) {
        [av_audio_capture.samplesArrivedSignal wait];
        byteSampleBuffer = TPCircularBufferTail(&av_audio_capture->audioSampleBuffer, &length);
      }

      const float *sampleBuffer = (float *) byteSampleBuffer;
      std::vector<float> vectorBuffer(sampleBuffer, sampleBuffer + sample_size);

      std::copy_n(std::begin(vectorBuffer), sample_size, std::begin(sample_in));

      TPCircularBufferConsume(&av_audio_capture->audioSampleBuffer, sample_size * sizeof(float));

      return capture_e::ok;
    }
  };

  struct macos_audio_control_t: public audio_control_t {
    AVCaptureDevice *audio_capture_device {};

  public:
    int
    set_sink(const std::string &sink) override {
      BOOST_LOG(warning) << "audio_control_t::set_sink() unimplemented: "sv << sink;
      return 0;
    }

    std::unique_ptr<mic_t>
    microphone(const std::uint8_t *mapping, int channels, std::uint32_t sample_rate, std::uint32_t frame_size, bool continuous_audio) override {
      auto mic = std::make_unique<av_mic_t>();
      const char *audio_sink = "";

      if (!request_microphone_permission()) {
        return nullptr;
      }

      if (!config::audio.sink.empty()) {
        audio_sink = config::audio.sink.c_str();
      }

      if ((audio_capture_device = [AVAudio findMicrophone:[NSString stringWithUTF8String:audio_sink]]) == nullptr) {
        BOOST_LOG(error) << "opening microphone '"sv << audio_sink << "' failed. Please set a valid input source in the Sunshine config."sv;
        BOOST_LOG(error) << "Available inputs:"sv;

        for (NSString *name in [AVAudio microphoneNames]) {
          BOOST_LOG(error) << "\t"sv << [name UTF8String];
        }

        return nullptr;
      }

      mic->av_audio_capture = [[AVAudio alloc] init];

      if ([mic->av_audio_capture setupMicrophone:audio_capture_device sampleRate:sample_rate frameSize:frame_size channels:channels]) {
        BOOST_LOG(error) << "Failed to setup microphone."sv;
        return nullptr;
      }

      return mic;
    }

    std::optional<sink_t>
    sink_info() override {
      sink_t sink;

      return sink;
    }

    bool
    is_sink_available(const std::string &sink) override {
      BOOST_LOG(warning) << "audio_control_t::is_sink_available() unimplemented: "sv << sink;
      return true;
    }

    int
    write_mic_pcm(const std::int16_t *samples, std::size_t frame_count) override {
      // Microphone redirect to the host is not implemented on macOS yet.
      (void) samples;
      (void) frame_count;
      return -1;
    }

    int
    init_mic_redirect_device() override {
      // No host-side virtual mic on macOS.
      return -1;
    }

    void
    release_mic_redirect_device() override {
      // Nothing to release.
    }
  };

  std::unique_ptr<audio_control_t>
  audio_control() {
    return std::make_unique<macos_audio_control_t>();
  }

  std::unique_ptr<deinit_t>
  init_audio_thread() {
    return std::make_unique<deinit_t>();
  }
}  // namespace platf
