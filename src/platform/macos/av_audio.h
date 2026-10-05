/**
 * @file src/platform/macos/av_audio.h
 * @brief Declarations for audio capture on macOS.
 */
#pragma once

#import <AVFoundation/AVFoundation.h>

#ifdef __cplusplus
  #include <functional>
#endif

#include "third-party/TPCircularBuffer/TPCircularBuffer.h"

#define kBufferLength 4096

@interface AVAudio: NSObject <AVCaptureAudioDataOutputSampleBufferDelegate> {
@public
  TPCircularBuffer audioSampleBuffer;
}

@property (nonatomic, assign) AVCaptureSession *audioCaptureSession;
@property (nonatomic, assign) AVCaptureConnection *audioConnection;
@property (nonatomic, assign) NSCondition *samplesArrivedSignal;

+ (NSArray *)microphoneNames;
+ (AVCaptureDevice *)findMicrophone:(NSString *)name;

- (int)setupMicrophone:(AVCaptureDevice *)device sampleRate:(UInt32)sampleRate frameSize:(UInt32)frameSize channels:(UInt8)channels;

@end

#ifdef __cplusplus
namespace platf {
  using microphone_permission_callback_t = std::function<void(bool)>;
  using microphone_permission_request_t = std::function<void(microphone_permission_callback_t)>;

  /**
   * @brief Resolve microphone access from an AVFoundation authorization state.
   * @param authorization_status Current authorization state for audio capture.
   * @param request_access Function used to request access when authorization has not been determined.
   * @return True when microphone access is authorized.
   */
  bool
  request_microphone_permission(AVAuthorizationStatus authorization_status, const microphone_permission_request_t &request_access);

  /**
   * @brief Ensure Sunshine has permission to capture microphone audio.
   * @return True when microphone access is authorized.
   */
  bool
  request_microphone_permission();
}  // namespace platf
#endif
