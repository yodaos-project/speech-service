#include "keepalive.h"
#include "flora-cli.h"
#include "rlog.h"
#include "defs.h"

using namespace std;

typedef struct {
	uint32_t type;
	const char* name;
} FloraEventInfo;

static FloraEventInfo flora_events[] = {
	{ FLORA_MSGTYPE_PERSIST, "rokid.speech.prepare_options" },
	{ FLORA_MSGTYPE_PERSIST, "rokid.speech.speech_options" },
	{ FLORA_MSGTYPE_PERSIST, "rokid.speech.stack" },
	{ FLORA_MSGTYPE_INSTANT, "rokid.turen.start_voice" },
	{ FLORA_MSGTYPE_INSTANT, "rokid.turen.voice" },
	{ FLORA_MSGTYPE_INSTANT, "rokid.turen.sleep" },
};

void Keepalive::run(CmdlineArgs& args, EventHandler& event_handler) {
	shared_ptr<flora::Client> flora_cli;
	int32_t r;
	unique_lock<mutex> locker(event_handler.reconn_mutex);

	while (true) {
		r = flora::Client::connect(args.flora_uri.c_str(), &event_handler,
				args.flora_bufsize, flora_cli);
		if (r != FLORA_CLI_SUCCESS) {
			KLOGI(TAG, "connect to flora service %s failed, retry after %u milliseconds",
					args.flora_uri.c_str(), args.flora_reconn_interval.count());
			event_handler.reconn_cond.wait_for(locker, args.flora_reconn_interval);
		} else {
			KLOGI(TAG, "flora service %s connected", args.flora_uri.c_str());
			subscribe_events(flora_cli);
			event_handler.set_flora_client(flora_cli);
			event_handler.reconn_cond.wait(locker);
		}
	}
}

void Keepalive::subscribe_events(shared_ptr<flora::Client>& cli) {
	int32_t i;
	int32_t count = sizeof(flora_events) / sizeof(FloraEventInfo);

	for (i = 0; i < count; ++i) {
		cli->subscribe(flora_events[i].name, flora_events[i].type);
	}
}
