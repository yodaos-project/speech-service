#pragma once

#include "defs.h"
#include "event-handler.h"

class Keepalive {
public:
	void run(CmdlineArgs& args, EventHandler& event_handler);

private:
	void subscribe_events(std::shared_ptr<flora::Client>& cli);
};
