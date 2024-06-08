#include "Broadcaster.hpp"
#include "MediaStreamTrackFactory.hpp"
#include "mediasoupclient.hpp"
#include "json.hpp"
#include <chrono>
#include <cpr/cpr.h>
#include <cstdlib>
#include <ctime>
#include <functional>
#include <iostream>
#include <string>
#include <thread>

using json = nlohmann::json;

Broadcaster::~Broadcaster()
{
	this->Stop();
}

void Broadcaster::OnTransportClose(mediasoupclient::Producer* /*producer*/)
{
	std::cout << "[INFO] Broadcaster::OnTransportClose()" << std::endl;
}

void Broadcaster::OnTransportClose(mediasoupclient::DataProducer* /*dataProducer*/)
{
	std::cout << "[INFO] Broadcaster::OnTransportClose()" << std::endl;
}

/* Transport::Listener::OnConnect
 *
 * Fired for the first Transport::Consume() or Transport::Produce().
 * Update the already created remote transport with the local DTLS parameters.
 */
std::future<void> Broadcaster::OnConnect(mediasoupclient::Transport* transport, const json& dtlsParameters)
{
	std::cout << "[INFO] Broadcaster::OnConnect()" << std::endl;
	// std::cout << "[INFO] dtlsParameters: " << dtlsParameters.dump(4) << std::endl;

	if (transport->GetId() == this->sendTransport->GetId())
	{
		return this->OnConnectSendTransport(dtlsParameters);
	}
	else if (transport->GetId() == this->recvTransport->GetId())
	{
		return this->OnConnectRecvTransport(dtlsParameters);
	}
	else
	{
		std::promise<void> promise;

		promise.set_exception(std::make_exception_ptr("Unknown transport requested to connect"));

		return promise.get_future();
	}
}

std::future<void> Broadcaster::OnConnectSendTransport(const json& dtlsParameters)
{
	std::promise<void> promise;

	/* clang-format off */
	json body =
	{
		{ "dtlsParameters", dtlsParameters }
	};
	/* clang-format on */

	auto r = cpr::PostAsync(
	           cpr::Url{ this->baseUrl + "/broadcasters/" + this->id + "/transports/" +
	                     this->sendTransport->GetId() + "/connect" },
	           cpr::Body{ body.dump() },
	           cpr::Header{ { "Content-Type", "application/json" } },
	           cpr::VerifySsl{ verifySsl })
	           .get();

	if (r.status_code == 200)
	{
		promise.set_value();
	}
	else
	{
		std::cerr << "[ERROR] unable to connect transport"
		          << " [status code:" << r.status_code << ", body:\"" << r.text << "\"]" << std::endl;

		promise.set_exception(std::make_exception_ptr(r.text));
	}

	return promise.get_future();
}

std::future<void> Broadcaster::OnConnectRecvTransport(const json& dtlsParameters)
{
	std::promise<void> promise;

	/* clang-format off */
	json body =
	{
		{ "dtlsParameters", dtlsParameters }
	};
	/* clang-format on */

	auto r = cpr::PostAsync(
	           cpr::Url{ this->baseUrl + "/broadcasters/" + this->id + "/transports/" +
	                     this->recvTransport->GetId() + "/connect" },
	           cpr::Body{ body.dump() },
	           cpr::Header{ { "Content-Type", "application/json" } },
	           cpr::VerifySsl{ verifySsl })
	           .get();

	if (r.status_code == 200)
	{
		promise.set_value();
	}
	else
	{
		std::cerr << "[ERROR] unable to connect transport"
		          << " [status code:" << r.status_code << ", body:\"" << r.text << "\"]" << std::endl;

		promise.set_exception(std::make_exception_ptr(r.text));
	}

	return promise.get_future();
}

/*
 * Transport::Listener::OnConnectionStateChange.
 */
void Broadcaster::OnConnectionStateChange(
  mediasoupclient::Transport* /*transport*/, const std::string& connectionState)
{
	std::cout << "[INFO] Broadcaster::OnConnectionStateChange() [connectionState:" << connectionState
	          << "]" << std::endl;

	if (connectionState == "failed")
	{
		Stop();
		std::exit(0);
	}
}

/* Producer::Listener::OnProduce
 *
 * Fired when a producer needs to be created in mediasoup.
 * Retrieve the remote producer ID and feed the caller with it.
 */
std::future<std::string> Broadcaster::OnProduce(
  mediasoupclient::SendTransport* /*transport*/,
  const std::string& kind,
  json rtpParameters,
  const json& /*appData*/)
{
	std::cout << "[INFO] Broadcaster::OnProduce()" << std::endl;
	// std::cout << "[INFO] rtpParameters: " << rtpParameters.dump(4) << std::endl;

	std::promise<std::string> promise;

	/* clang-format off */
	json body =
	{
		{ "kind",          kind          },
		{ "rtpParameters", rtpParameters }
	};
	/* clang-format on */

	auto r = cpr::PostAsync(
	           cpr::Url{ this->baseUrl + "/broadcasters/" + this->id + "/transports/" +
	                     this->sendTransport->GetId() + "/producers" },
	           cpr::Body{ body.dump() },
	           cpr::Header{ { "Content-Type", "application/json" } },
	           cpr::VerifySsl{ verifySsl })
	           .get();

	if (r.status_code == 200)
	{
		auto response = json::parse(r.text);

		auto it = response.find("id");
		if (it == response.end() || !it->is_string())
		{
			promise.set_exception(std::make_exception_ptr("'id' missing in response"));
		}

		promise.set_value((*it).get<std::string>());
	}
	else
	{
		std::cerr << "[ERROR] unable to create producer"
		          << " [status code:" << r.status_code << ", body:\"" << r.text << "\"]" << std::endl;

		promise.set_exception(std::make_exception_ptr(r.text));
	}

	return promise.get_future();
}

/* Producer::Listener::OnProduceData
 *
 * Fired when a data producer needs to be created in mediasoup.
 * Retrieve the remote producer ID and feed the caller with it.
 */
std::future<std::string> Broadcaster::OnProduceData(
  mediasoupclient::SendTransport* /*transport*/,
  const json& sctpStreamParameters,
  const std::string& label,
  const std::string& protocol,
  const json& /*appData*/)
{
	std::cout << "[INFO] Broadcaster::OnProduceData()" << std::endl;
	// std::cout << "[INFO] rtpParameters: " << rtpParameters.dump(4) << std::endl;

	std::promise<std::string> promise;

	/* clang-format off */
	json body =
    {
        { "label"                , label },
        { "protocol"             , protocol },
        { "sctpStreamParameters" , sctpStreamParameters }
		// { "appData"				 , "someAppData" }
	};
	/* clang-format on */

	auto r = cpr::PostAsync(
	           cpr::Url{ this->baseUrl + "/broadcasters/" + this->id + "/transports/" +
	                     this->sendTransport->GetId() + "/produce/data" },
	           cpr::Body{ body.dump() },
	           cpr::Header{ { "Content-Type", "application/json" } },
	           cpr::VerifySsl{ verifySsl })
	           .get();

	if (r.status_code == 200)
	{
		auto response = json::parse(r.text);

		auto it = response.find("id");
		if (it == response.end() || !it->is_string())
		{
			promise.set_exception(std::make_exception_ptr("'id' missing in response"));
		}
		else
		{
			auto dataProducerId = (*it).get<std::string>();
			promise.set_value(dataProducerId);
		}
	}
	else
	{
		std::cerr << "[ERROR] unable to create data producer"
		          << " [status code:" << r.status_code << ", body:\"" << r.text << "\"]" << std::endl;

		promise.set_exception(std::make_exception_ptr(r.text));
	}

	return promise.get_future();
}

void Broadcaster::Start(
  const std::string& baseUrl,
  bool enableAudio,
  bool useSimulcast,
  const json& routerRtpCapabilities,
  bool verifySsl)
{
	std::cout << "[INFO] Broadcaster::Start()" << std::endl;

	this->baseUrl   = baseUrl;
	this->verifySsl = verifySsl;

	// Load the device.
	this->device.Load(routerRtpCapabilities);

	std::cout << "[INFO] creating Broadcaster..." << std::endl;

	/* clang-format off */
	json body =
	{
		{ "id",          this->id          },
		{ "displayName", "broadcaster"     },
		{ "device",
			{
				{ "name",    "libmediasoupclient"       },
				{ "version", mediasoupclient::Version() }
			}
		},
		{ "rtpCapabilities", this->device.GetRtpCapabilities() }
	};
	/* clang-format on */

	auto r = cpr::PostAsync(
	           cpr::Url{ this->baseUrl + "/broadcasters" },
	           cpr::Body{ body.dump() },
	           cpr::Header{ { "Content-Type", "application/json" } },
	           cpr::VerifySsl{ verifySsl })
	           .get();

	if (r.status_code != 200)
	{
		std::cerr << "[ERROR] unable to create Broadcaster"
		          << " [status code:" << r.status_code << ", body:\"" << r.text << "\"]" << std::endl;

		return;
	}

	this->CreateSendTransport(enableAudio, useSimulcast);
	this->CreateRecvTransport();
}

void Broadcaster::CreateDataConsumer()
{
	const std::string& dataProducerId = this->dataProducer->GetId();

	/* clang-format off */
	json body =
	{
		{ "dataProducerId", dataProducerId }
	};
	/* clang-format on */
	// create server data consumer
	auto r = cpr::PostAsync(
	           cpr::Url{ this->baseUrl + "/broadcasters/" + this->id + "/transports/" +
	                     this->recvTransport->GetId() + "/consume/data" },
	           cpr::Body{ body.dump() },
	           cpr::Header{ { "Content-Type", "application/json" } },
	           cpr::VerifySsl{ verifySsl })
	           .get();
	if (r.status_code != 200)
	{
		std::cerr << "[ERROR] server unable to consume mediasoup recv WebRtcTransport"
		          << " [status code:" << r.status_code << ", body:\"" << r.text << "\"]" << std::endl;
		return;
	}

	auto response = json::parse(r.text);
	if (response.find("id") == response.end())
	{
		std::cerr << "[ERROR] 'id' missing in response" << std::endl;
		return;
	}
	auto dataConsumerId = response["id"].get<std::string>();

	if (response.find("streamId") == response.end())
	{
		std::cerr << "[ERROR] 'streamId' missing in response" << std::endl;
		return;
	}
	auto streamId = response["streamId"].get<uint16_t>();

	// Create client consumer.
	this->dataConsumer = this->recvTransport->ConsumeData(
	  this, dataConsumerId, dataProducerId, streamId, "chat", "", nlohmann::json());
}

void Broadcaster::CreateSendTransport(bool enableAudio, bool useSimulcast)
{
	std::cout << "[INFO] creating mediasoup send WebRtcTransport..." << std::endl;

	json sctpCapabilities = this->device.GetSctpCapabilities();
	/* clang-format off */
	json body =
	{
		{ "type",    "webrtc" },
		{ "rtcpMux", true     },
		{ "sctpCapabilities", sctpCapabilities }
	};
	/* clang-format on */

	auto r = cpr::PostAsync(
	           cpr::Url{ this->baseUrl + "/broadcasters/" + this->id + "/transports" },
	           cpr::Body{ body.dump() },
	           cpr::Header{ { "Content-Type", "application/json" } },
	           cpr::VerifySsl{ verifySsl })
	           .get();

	if (r.status_code != 200)
	{
		std::cerr << "[ERROR] unable to create send mediasoup WebRtcTransport"
		          << " [status code:" << r.status_code << ", body:\"" << r.text << "\"]" << std::endl;

		return;
	}

	auto response = json::parse(r.text);

	if (response.find("id") == response.end())
	{
		std::cerr << "[ERROR] 'id' missing in response" << std::endl;

		return;
	}
	else if (response.find("iceParameters") == response.end())
	{
		std::cerr << "[ERROR] 'iceParametersd' missing in response" << std::endl;

		return;
	}
	else if (response.find("iceCandidates") == response.end())
	{
		std::cerr << "[ERROR] 'iceCandidates' missing in response" << std::endl;

		return;
	}
	else if (response.find("dtlsParameters") == response.end())
	{
		std::cerr << "[ERROR] 'dtlsParameters' missing in response" << std::endl;

		return;
	}
	else if (response.find("sctpParameters") == response.end())
	{
		std::cerr << "[ERROR] 'sctpParameters' missing in response" << std::endl;

		return;
	}

	std::cout << "[INFO] creating SendTransport..." << std::endl;

	auto sendTransportId = response["id"].get<std::string>();

	this->sendTransport = this->device.CreateSendTransport(
	  this,
	  sendTransportId,
	  response["iceParameters"],
	  response["iceCandidates"],
	  response["dtlsParameters"],
	  response["sctpParameters"]);

	///////////////////////// Create Audio Producer //////////////////////////

	if (enableAudio && this->device.CanProduce("audio"))
	{
		auto audioTrack = createAudioTrack(std::to_string(rtc::CreateRandomId()));

		/* clang-format off */
		json codecOptions = {
			{ "opusStereo", true },
			{ "opusDtx",		true }
		};
		/* clang-format on */

		this->sendTransport->Produce(this, audioTrack.get(), nullptr, &codecOptions, nullptr);
	}
	else
	{
		std::cerr << "[WARN] cannot produce audio" << std::endl;
	}

	///////////////////////// Create Video Producer //////////////////////////

	if (this->device.CanProduce("video"))
	{
		auto videoTrack = createSquaresVideoTrack(std::to_string(rtc::CreateRandomId()));

		if (useSimulcast)
		{
			std::vector<webrtc::RtpEncodingParameters> encodings;
			encodings.emplace_back(webrtc::RtpEncodingParameters());
			encodings.emplace_back(webrtc::RtpEncodingParameters());
			encodings.emplace_back(webrtc::RtpEncodingParameters());

			this->sendTransport->Produce(this, videoTrack.get(), &encodings, nullptr, nullptr);
		}
		else
		{
			this->sendTransport->Produce(this, videoTrack.get(), nullptr, nullptr, nullptr);
		}
	}
	else
	{
		std::cerr << "[WARN] cannot produce video" << std::endl;

		return;
	}

	///////////////////////// Create Data Producer //////////////////////////

	this->dataProducer = sendTransport->ProduceData(this);

	uint32_t intervalSeconds = 10;
	std::thread([this, intervalSeconds]() {
		bool run = true;
		while (run)
		{
			std::chrono::system_clock::time_point p = std::chrono::system_clock::now();
			std::time_t t                           = std::chrono::system_clock::to_time_t(p);
			std::string s                           = std::ctime(&t);
			auto dataBuffer                         = webrtc::DataBuffer(s);
			std::cout << "[INFO] sending chat data: " << s << std::endl;
			this->dataProducer->Send(dataBuffer);
			run = timerKiller.WaitFor(std::chrono::seconds(intervalSeconds));
		}
	})
	  .detach();
}

void Broadcaster::CreateRecvTransport()
{
	std::cout << "[INFO] creating mediasoup recv WebRtcTransport..." << std::endl;

	json sctpCapabilities = this->device.GetSctpCapabilities();
	/* clang-format off */
	json body =
	{
		{ "type",    "webrtc" },
		{ "rtcpMux", true     },
		{ "sctpCapabilities", sctpCapabilities }
	};
	/* clang-format on */

	// create server transport
	auto r = cpr::PostAsync(
	           cpr::Url{ this->baseUrl + "/broadcasters/" + this->id + "/transports" },
	           cpr::Body{ body.dump() },
	           cpr::Header{ { "Content-Type", "application/json" } },
	           cpr::VerifySsl{ verifySsl })
	           .get();

	if (r.status_code != 200)
	{
		std::cerr << "[ERROR] unable to create mediasoup recv WebRtcTransport"
		          << " [status code:" << r.status_code << ", body:\"" << r.text << "\"]" << std::endl;

		return;
	}

	auto response = json::parse(r.text);

	if (response.find("id") == response.end())
	{
		std::cerr << "[ERROR] 'id' missing in response" << std::endl;

		return;
	}
	else if (response.find("iceParameters") == response.end())
	{
		std::cerr << "[ERROR] 'iceParameters' missing in response" << std::endl;

		return;
	}
	else if (response.find("iceCandidates") == response.end())
	{
		std::cerr << "[ERROR] 'iceCandidates' missing in response" << std::endl;

		return;
	}
	else if (response.find("dtlsParameters") == response.end())
	{
		std::cerr << "[ERROR] 'dtlsParameters' missing in response" << std::endl;

		return;
	}
	else if (response.find("sctpParameters") == response.end())
	{
		std::cerr << "[ERROR] 'sctpParameters' missing in response" << std::endl;

		return;
	}

	auto recvTransportId = response["id"].get<std::string>();

	std::cout << "[INFO] creating RecvTransport..." << std::endl;

	auto sctpParameters = response["sctpParameters"];

	this->recvTransport = this->device.CreateRecvTransport(
	  this,
	  recvTransportId,
	  response["iceParameters"],
	  response["iceCandidates"],
	  response["dtlsParameters"],
	  sctpParameters);

	this->CreateDataConsumer();
}

void Broadcaster::OnMessage(mediasoupclient::DataConsumer* dataConsumer, const webrtc::DataBuffer& buffer)
{
	std::cout << "[INFO] Broadcaster::OnMessage()" << std::endl;
	if (dataConsumer->GetLabel() == "chat")
	{
		std::string s = std::string(buffer.data.data<char>(), buffer.data.size());
		std::cout << "[INFO] received chat data: " + s << std::endl;
	}
}

void Broadcaster::Stop()
{
	std::cout << "[INFO] Broadcaster::Stop()" << std::endl;

	this->timerKiller.Kill();

	if (this->recvTransport)
	{
		recvTransport->Close();
	}

	if (this->sendTransport)
	{
		sendTransport->Close();
	}

	cpr::DeleteAsync(
	  cpr::Url{ this->baseUrl + "/broadcasters/" + this->id }, cpr::VerifySsl{ verifySsl })
	  .get();
}

void Broadcaster::OnOpen(mediasoupclient::DataProducer* /*dataProducer*/)
{
	std::cout << "[INFO] Broadcaster::OnOpen()" << std::endl;
}
void Broadcaster::OnClose(mediasoupclient::DataProducer* /*dataProducer*/)
{
	std::cout << "[INFO] Broadcaster::OnClose()" << std::endl;
}
void Broadcaster::OnBufferedAmountChange(mediasoupclient::DataProducer* /*dataProducer*/, uint64_t /*size*/)
{
	std::cout << "[INFO] Broadcaster::OnBufferedAmountChange()" << std::endl;
}
