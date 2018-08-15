#pragma once

#include <mutex>
#include <condition_variable>
#include "flora-cli.h"

class InfiniteVoice : public flora::ClientCallback {
public:
	void run(std::shared_ptr<flora::Client>& cli);


	void recv_post(const char* name, uint32_t msgtype, std::shared_ptr<Caps>& msg);

private:
	int32_t turen_id = 0;
	std::mutex id_mutex;
	std::condition_variable id_cond;
};
