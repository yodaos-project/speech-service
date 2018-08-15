#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <chrono>
#include "infinite-voice.h"
#include "rlog.h"

#define VOICE_FILE "./test.pcm"
#define TAG "speech-service.demo"

using namespace std;

static int8_t* open_input_voice(const char* fname, uint32_t& size) {
	int fd = open(fname, O_RDONLY);
	if (fd < 0)
		return nullptr;
	size = lseek(fd, 0, SEEK_END);
	void* data = mmap(nullptr, size, PROT_READ, MAP_PRIVATE | MAP_FILE, fd, 0);
	::close(fd);
	if (data == nullptr)
		return nullptr;
	return (int8_t*)data;
}

void InfiniteVoice::run(shared_ptr<flora::Client>& cli) {
	uint32_t size;
	int8_t* data = open_input_voice(VOICE_FILE, size);
	uint32_t frame_size = 16 * sizeof(float) * 80;
	if (data == nullptr) {
		KLOGE(TAG, "open voice file %s failed", VOICE_FILE);
		return;
	}
	if (size < frame_size) {
		KLOGE(TAG, "voice file %s too small, size %u, should > %u",
				VOICE_FILE, size, frame_size);
		return;
	}

	shared_ptr<Caps> msg;
	int32_t idseq = 0;
	uint32_t off = 0;
	int32_t tuid;
	unique_lock<mutex> locker(id_mutex, defer_lock);
	while (true) {
		locker.lock();
		if (turen_id < 0) {
			id_cond.wait(locker);
			locker.unlock();
			continue;
		}
		tuid = turen_id;
		locker.unlock();
		if (tuid == 0) {
			id_mutex.lock();
			turen_id = ++idseq;
			tuid = turen_id;
			id_mutex.unlock();
			KLOGI(TAG, "post turen awake, turen id = %d", tuid);
			msg = Caps::new_instance();
			msg->write("");
			msg->write((int32_t)0);
			msg->write((int32_t)0);
			msg->write(0.0f);
			msg->write((int32_t)0);
			msg->write(tuid);
			cli->post("rokid.turen.awake", msg, FLORA_MSGTYPE_INSTANT);
		}
		if (off + frame_size > size)
			off = 0;
		msg = Caps::new_instance();
		msg->write(data + off, frame_size);
		off += frame_size;
		usleep(80000);
		cli->post("rokid.turen.voice", msg, FLORA_MSGTYPE_INSTANT);
	}
}

void InfiniteVoice::recv_post(const char* name, uint32_t msgtype, std::shared_ptr<Caps>& msg) {
	if (strcmp(name, "rokid.speech.final_asr") == 0) {
		string asr;
		int32_t tuid;
		int32_t rtuid = -1;

		msg->read_string(asr);
		msg->read(rtuid);
		KLOGI(TAG, "recv final_asr %s, id %d", asr.c_str(), rtuid);
		id_mutex.lock();
		tuid = turen_id;
		turen_id = -1;
		id_mutex.unlock();
		if (tuid != rtuid)
			KLOGE(TAG, "received turen id %d, not equal %d", rtuid, tuid);
	} else if (strcmp(name, "rokid.speech.nlp") == 0) {
		string nlp;
		string action;

		id_mutex.lock();
		turen_id = 0;
		id_cond.notify_one();
		id_mutex.unlock();

		msg->read_string(nlp);
		msg->read_string(action);
		KLOGI(TAG, "recv nlp:\n%s\naction:\n%s", nlp.c_str(), action.c_str());
	} else if (strcmp(name, "rokid.speech.error") == 0) {
		int32_t err;
		int32_t rtuid = -1;
		int32_t tuid;

		msg->read(err);
		msg->read(rtuid);
		KLOGI(TAG, "recv error: %d, turen id %d", err, rtuid);
		id_mutex.lock();
		tuid = turen_id;
		turen_id = 0;
		id_cond.notify_one();
		id_mutex.unlock();
		if (tuid != rtuid)
			KLOGE(TAG, "received turen id %d, not equal %d", rtuid, tuid);
	}
}
