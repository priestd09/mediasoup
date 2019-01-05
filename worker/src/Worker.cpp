#define MS_CLASS "Worker"
// #define MS_LOG_DEV

#include "Worker.hpp"
#include "DepLibUV.hpp"
#include "Logger.hpp"
#include "MediaSoupError.hpp"
#include "Settings.hpp"
#include "Channel/Notifier.hpp"

/* Instance methods. */

Worker::Worker(Channel::UnixStreamSocket* channel) : channel(channel)
{
	MS_TRACE();

	// Set us as Channel's listener.
	this->channel->SetListener(this);

	// Set the signals handler.
	this->signalsHandler = new SignalsHandler(this);

	// Add signals to handle.
	this->signalsHandler->AddSignal(SIGINT, "INT");
	this->signalsHandler->AddSignal(SIGTERM, "TERM");

	// Tell the Node process that we are running.
	Channel::Notifier::Emit(std::to_string(Logger::pid), "running");

	MS_DEBUG_DEV("starting libuv loop");
	DepLibUV::RunLoop();
	MS_DEBUG_DEV("libuv loop ended");
}

Worker::~Worker()
{
	MS_TRACE();

	if (!this->closed)
		Close();
}

void Worker::Close()
{
	MS_TRACE();

	if (this->closed)
		return;

	this->closed = true;

	// Close the SignalsHandler.
	delete this->signalsHandler;

	// Close all Routers.
	// for (auto& kv : this->routers)
	// {
	// 	auto* router = kv.second;

	// 	delete router;
	// }
	// this->routers.clear();

	// Close the Channel.
	delete this->channel;
}

void Worker::FillJson(json& jsonObject) const
{
	MS_TRACE();

	// Add pid.
	jsonObject["pid"] = Logger::pid;

	// Add routerIds.
	jsonObject["routerIds"] = json::array();
	auto jsonRouterIdsIt    = jsonObject.find("routerIds");

	// TODO
	// for (auto& kv : this->routers)
	// {
	// 	auto& routerId = kv.first;

	// 	jsonRouterIdsIt->emplace_back(routerId);
	// }
}

// void Worker::SetNewRouterIdFromRequest(Channel::Request* request, std::string& routerId) const
// {
// 	MS_TRACE();

// 	auto jsonRouterIdIt = request->internal.find("routerId");

// 	if (jsonRouterIdIt == request->internal.end() || !jsonRouterIdIt->is_string())
// 		MS_THROW_ERROR("request has no internal.routerId");

// 	routerId.assign(jsonRouterIdIt->get<std::string>());

// 	if (this->routers.find(routerId) != this->routers.end())
// 		MS_THROW_ERROR("a Router with same routerId already exists");
// }

// RTC::Router* Worker::GetRouterFromRequest(Channel::Request* request) const
// {
// 	MS_TRACE();

// 	auto jsonRouterIdIt = request->internal.find("routerId");

// 	if (jsonRouterIdIt == request->internal.end() || !jsonRouterIdIt->is_string())
// 		MS_THROW_ERROR("request has no internal.routerId");

// 	auto it = this->routers.find(jsonRouterIdIt->get<std::string>());

// 	if (it == this->routers.end())
// 		MS_THROW_ERROR("Router not found");

// 	RTC::Router* router = it->second;

// 	return router;
// }

void Worker::OnChannelRequest(Channel::UnixStreamSocket* /*channel*/, Channel::Request* request)
{
	MS_TRACE();

	MS_DEBUG_DEV(
		"Channel request received [method:%s, id:%" PRIu32 "]", request->method.c_str(), request->id);

	switch (request->methodId)
	{
		case Channel::Request::MethodId::WORKER_DUMP:
		{
			json data = json::object();

			FillJson(data);

			request->Accept(data);

			break;
		}

		case Channel::Request::MethodId::WORKER_UPDATE_SETTINGS:
		{
			Settings::HandleRequest(request);

			break;
		}

		// case Channel::Request::MethodId::WORKER_CREATE_ROUTER:
		// {
		// 	std::string routerId;

		// 	try
		// 	{
		// 		SetNewRouterIdFromRequest(request, routerId);
		// 	}
		// 	catch (const MediaSoupError& error)
		// 	{
		// 		request->Reject(error.what());

		// 		break;
		// 	}

		// 	auto* router = new RTC::Router(routerId);

		// 	this->routers[routerId] = router;

		// 	MS_DEBUG_DEV("Router created [routerId:%s]", routerId.c_str());

		// 	request->Accept();

		// 	break;
		// }

		// case Channel::Request::MethodId::ROUTER_CLOSE:
		// {
		// 	RTC::Router* router;

		// 	try
		// 	{
		// 		router = GetRouterFromRequest(request);
		// 	}
		// 	catch (const MediaSoupError& error)
		// 	{
		// 		request->Reject(error.what());

		// 		break;
		// 	}

		// 	// Remove it from the map and delete it.
		// 	this->routers.erase(router->routerId);
		// 	delete router;

		// 	request->Accept();

		// 	break;
		// }

		// default:
		// {
		// 	RTC::Router* router;

		// 	try
		// 	{
		// 		router = GetRouterFromRequest(request);
		// 	}
		// 	catch (const MediaSoupError& error)
		// 	{
		// 		request->Reject(error.what());

		// 		break;
		// 	}

		// 	router->HandleRequest(request);

		// 	break;
		// }
	}
}

void Worker::OnChannelRemotelyClosed(Channel::UnixStreamSocket* /*socket*/)
{
	MS_TRACE_STD();

	// If the pipe is remotely closed it means that mediasoup Node process
	// abruptly died (SIGKILL?) so we must die.
	MS_ERROR_STD("channel remotely closed, closing myself");

	Close();
}

void Worker::OnSignal(SignalsHandler* /*signalsHandler*/, int signum)
{
	MS_TRACE();

	switch (signum)
	{
		case SIGINT:
		{
			MS_DEBUG_DEV("INT signal received, closing myself");

			Close();

			break;
		}

		case SIGTERM:
		{
			MS_DEBUG_DEV("TERM signal received, closing myself");

			Close();

			break;
		}

		default:
		{
			MS_WARN_DEV("received a non handled signal [signum:%d]", signum);
		}
	}
}
