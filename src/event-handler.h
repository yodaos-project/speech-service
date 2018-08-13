#pragma once

#include <mutex>
#include <condition_variable>
#include <map>
#include "defs.h"
#include "speech.h"
#include "flora-cli.h"

class EventHandler;
typedef void (EventHandler::*EventHandleFunc)(uint32_t msgtype, std::shared_ptr<Caps>& msg);
typedef std::map<std::string, EventHandleFunc> EventHandlerMap;

class EventHandler : public flora::ClientCallback {
public:
	void init(CmdlineArgs& args);

	void set_flora_client(std::shared_ptr<flora::Client>& cli);


	// callbacks of flora::ClientCallback
	void recv_post(const char* name, uint32_t msgtype,
			std::shared_ptr<Caps>& msg);

	void disconnected();

public:
	void handle_speech_prepare_options(uint32_t msgtype, std::shared_ptr<Caps>& msg);
	void handle_speech_options(uint32_t msgtype, std::shared_ptr<Caps>& msg);
	void handle_speech_stack(uint32_t msgtype, std::shared_ptr<Caps>& msg);
	void handle_turen_awake(uint32_t msgtype, std::shared_ptr<Caps>& msg);
	void handle_turen_voice(uint32_t msgtype, std::shared_ptr<Caps>& msg);
	void handle_turen_sleep(uint32_t msgtype, std::shared_ptr<Caps>& msg);

private:
	void do_speech_poll();
	void flora_disconnected();

public:
	std::mutex reconn_mutex;
	std::condition_variable reconn_cond;
	std::shared_ptr<rokid::speech::Speech> speech;
	std::shared_ptr<flora::Client> flora_cli;
	EventHandlerMap handlers;
	std::string speech_stack;
	int32_t turen_id = 0;
	int32_t speech_id = 0;
	std::mutex speech_mutex;
	std::condition_variable speech_cond;
	bool speech_prepared = false;
};
