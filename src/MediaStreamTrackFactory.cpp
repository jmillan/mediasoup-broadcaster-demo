#define MSC_CLASS "MediaStreamTrackFactory"

#include <iostream>

#include "MediaSoupClientErrors.hpp"
#include "MediaStreamTrackFactory.hpp"
#include "pc/test/fake_audio_capture_module.h"
#include "pc/test/fake_periodic_video_track_source.h"
#include "pc/test/frame_generator_capturer_video_track_source.h"
#include "pc/test/fake_video_track_source.h"
#include "system_wrappers/include/clock.h"
#include "api/audio_codecs/builtin_audio_decoder_factory.h"
#include "api/audio_codecs/builtin_audio_encoder_factory.h"
#include "api/create_peerconnection_factory.h"
#include "api/video_codecs/builtin_video_decoder_factory.h"
#include "api/video_codecs/builtin_video_encoder_factory.h"

#include "api/video_codecs/video_decoder_factory.h"
#include "api/video_codecs/video_decoder_factory_template.h"
#include "api/video_codecs/video_decoder_factory_template_dav1d_adapter.h"
#include "api/video_codecs/video_decoder_factory_template_libvpx_vp8_adapter.h"
#include "api/video_codecs/video_decoder_factory_template_libvpx_vp9_adapter.h"
#include "api/video_codecs/video_decoder_factory_template_open_h264_adapter.h"
#include "api/video_codecs/video_encoder_factory.h"
#include "api/video_codecs/video_encoder_factory_template.h"
#include "api/video_codecs/video_encoder_factory_template_libaom_av1_adapter.h"
#include "api/video_codecs/video_encoder_factory_template_libvpx_vp8_adapter.h"
#include "api/video_codecs/video_encoder_factory_template_libvpx_vp9_adapter.h"
#include "api/video_codecs/video_encoder_factory_template_open_h264_adapter.h"

using namespace mediasoupclient;

static rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> factory;

/* MediaStreamTrack holds reference to the threads of the PeerConnectionFactory.
 * Use plain pointers in order to avoid threads being destructed before tracks.
 */
// static rtc::Thread* networkThread;
// static rtc::Thread* signalingThread;
  std::unique_ptr<rtc::Thread> signalingThread;
// static rtc::Thread* workerThread;

static void createFactory()
{
	// networkThread   = rtc::Thread::Create().release();
	signalingThread = rtc::Thread::CreateWithSocketServer();
	// workerThread    = rtc::Thread::Create().release();

	// networkThread->SetName("network_thread", nullptr);
	signalingThread->SetName("signaling_thread", nullptr);
	// workerThread->SetName("worker_thread", nullptr);

	// if (!networkThread->Start() || !signalingThread->Start() || !workerThread->Start())
	if(!signalingThread->Start())
	{
		MSC_THROW_INVALID_STATE_ERROR("thread start errored");
	}

	webrtc::PeerConnectionInterface::RTCConfiguration config;

	auto fakeAudioCaptureModule = FakeAudioCaptureModule::Create();
	if (!fakeAudioCaptureModule)
	{
		MSC_THROW_INVALID_STATE_ERROR("audio capture module creation errored");
	}

	factory = webrtc::CreatePeerConnectionFactory(
	  nullptr,
	  nullptr,
	  signalingThread.get(),
	  fakeAudioCaptureModule,
	  webrtc::CreateBuiltinAudioEncoderFactory(),
	  webrtc::CreateBuiltinAudioDecoderFactory(),
	//   webrtc::CreateBuiltinVideoEncoderFactory(),
	//   webrtc::CreateBuiltinVideoDecoderFactory(),
      std::make_unique<webrtc::VideoEncoderFactoryTemplate<
          webrtc::LibvpxVp8EncoderTemplateAdapter,
          webrtc::LibvpxVp9EncoderTemplateAdapter,
          webrtc::OpenH264EncoderTemplateAdapter,
          webrtc::LibaomAv1EncoderTemplateAdapter>>(),
      std::make_unique<webrtc::VideoDecoderFactoryTemplate<
          webrtc::LibvpxVp8DecoderTemplateAdapter,
          webrtc::LibvpxVp9DecoderTemplateAdapter,
          webrtc::OpenH264DecoderTemplateAdapter,
          webrtc::Dav1dDecoderTemplateAdapter>>(),
	  nullptr /*audio_mixer*/,
	  nullptr /*audio_processing*/);

	if (!factory)
	{
		MSC_THROW_ERROR("error ocurred creating peerconnection factory");
	}
}

// Audio track creation.
rtc::scoped_refptr<webrtc::AudioTrackInterface> createAudioTrack(const std::string& label)
{
	if (!factory)
		createFactory();

	cricket::AudioOptions options;
	options.highpass_filter = false;

	rtc::scoped_refptr<webrtc::AudioSourceInterface> source = factory->CreateAudioSource(options);

	return factory->CreateAudioTrack(label, source.get());
}

// Video track creation.
rtc::scoped_refptr<webrtc::VideoTrackInterface> createVideoTrack(const std::string& /*label*/)
{
	if (!factory)
		createFactory();

	auto* videoTrackSource =
	  new rtc::RefCountedObject<webrtc::FakePeriodicVideoTrackSource>(false /* remote */);

	return factory->CreateVideoTrack(rtc::CreateRandomUuid(), videoTrackSource);
}

rtc::scoped_refptr<webrtc::VideoTrackInterface> createSquaresVideoTrack(const std::string& /*label*/)
{
	if (!factory)
		createFactory();

	std::cout << "[INFO] getting frame generator" << std::endl;
	auto videoTrackSource = rtc::make_ref_counted<webrtc::FrameGeneratorCapturerVideoTrackSource>(
	  webrtc::FrameGeneratorCapturerVideoTrackSource::Config(), webrtc::Clock::GetRealTimeClock(), false);
	// videoTrackSource->Start();

	std::cout << "[INFO] creating video track" << std::endl;
	return factory->CreateVideoTrack(videoTrackSource, rtc::CreateRandomUuid());
	// return factory->CreateVideoTrack(webrtc::FakeVideoTrackSource::Create(), rtc::CreateRandomUuid());
}
