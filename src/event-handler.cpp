#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <thread>
#include "event-handler.h"
#include "defs.h"
#include "rlog.h"
#include "speech.h"
#include "uri.h"

#define DEFAULT_SPEECH_URI "wss://apigwws.open.rokid.com:443/api"

using namespace std;
using namespace rokid;
using namespace rokid::speech;

static int pcm_file = -1;
void open_pcm_file(const string& file) {
  if (file.length() > 0) {
    pcm_file = open(file.c_str(), O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if (pcm_file < 0) {
      KLOGW(TAG, "lastest speech file %s open failed", file.c_str());
    }
  }
}

void write_pcm_file(string& data) {
  if (pcm_file >= 0) {
    write(pcm_file, data.data(), data.length());
  }
}

void close_pcm_file() {
  if (pcm_file >= 0) {
    ::close(pcm_file);
    pcm_file = -1;
  }
}

void EventHandler::init(CmdlineArgs& args) {
  FloraMsgInfo key;

  lastest_speech_file = args.lastest_speech_file;

  key.name = "rokid.speech.prepare_options";
  key.type = FLORA_MSGTYPE_PERSIST;
  handlers.insert(make_pair(key, [this](shared_ptr<Caps>& msg) { this->handle_speech_prepare_options(msg); }));
  key.name = "rokid.speech.options";
  key.type = FLORA_MSGTYPE_PERSIST;
  handlers.insert(make_pair(key, [this](shared_ptr<Caps>& msg) { this->handle_speech_options(msg); }));
  key.name = "rokid.speech.stack";
  key.type = FLORA_MSGTYPE_PERSIST;
  handlers.insert(make_pair(key, [this](shared_ptr<Caps>& msg) { this->handle_speech_stack(msg); }));
  key.name = "rokid.turen.start_voice";
  key.type = FLORA_MSGTYPE_INSTANT;
  handlers.insert(make_pair(key, [this](shared_ptr<Caps>& msg) { this->handle_turen_start_voice(msg); }));
  key.name = "rokid.turen.voice";
  key.type = FLORA_MSGTYPE_INSTANT;
  handlers.insert(make_pair(key, [this](shared_ptr<Caps>& msg) { this->handle_turen_voice(msg); }));
  key.name = "rokid.turen.end_voice";
  key.type = FLORA_MSGTYPE_INSTANT;
  handlers.insert(make_pair(key, [this](shared_ptr<Caps>& msg) { this->handle_turen_end_voice(msg); }));
  key.name = "rokid.turen.sleep";
  key.type = FLORA_MSGTYPE_INSTANT;
  handlers.insert(make_pair(key, [this](shared_ptr<Caps>& msg) { this->handle_turen_sleep(msg); }));
  key.name = "rokid.speech.put_text";
  key.type = FLORA_MSGTYPE_INSTANT;
  handlers.insert(make_pair(key, [this](shared_ptr<Caps>& msg) { this->handle_speech_put_text(msg); }));

  speech = Speech::new_instance();
  std::thread speech_poll_thread([this]() { this->do_speech_poll(); });
  speech_poll_thread.detach();

  flora_keepalive(args);
}

void EventHandler::recv_post(const char* name, uint32_t msgtype,
    shared_ptr<Caps>& msg) {
  FloraMsgInfo key;
  key.name = name;
  key.type = msgtype;
  KLOGI(TAG, "recv_post *a*");
  EventHandlerMap::iterator it = handlers.find(key);
  KLOGI(TAG, "recv_post *b*");
  it->second(msg);
  KLOGI(TAG, "recv_post *c*");
}

void EventHandler::handle_speech_prepare_options(shared_ptr<Caps>& msg) {
  string uri;
  PrepareOptions popts;
  int32_t v;
  Uri uri_parser;

  if (msg->read_string(uri) != CAPS_SUCCESS) {
    goto msg_invalid;
  }
  if (uri.length() == 0)
    uri = DEFAULT_SPEECH_URI;
  KLOGI(TAG, "recv prepare options: uri = %s", uri.c_str());
  if (msg->read_string(popts.key) != CAPS_SUCCESS) {
    goto msg_invalid;
  }
  KLOGI(TAG, "recv prepare options: key = %s", popts.key.c_str());
  if (msg->read_string(popts.device_type_id) != CAPS_SUCCESS) {
    goto msg_invalid;
  }
  KLOGI(TAG, "recv prepare options: device type = %s", popts.device_type_id.c_str());
  if (msg->read_string(popts.secret) != CAPS_SUCCESS) {
    goto msg_invalid;
  }
  if (msg->read_string(popts.device_id) != CAPS_SUCCESS) {
    goto msg_invalid;
  }
  KLOGI(TAG, "recv prepare options: device id = %s", popts.device_id.c_str());
  if (msg->read(v) != CAPS_SUCCESS) {
    goto msg_invalid;
  }
  if (v > 0) {
    popts.reconn_interval = v;
    KLOGI(TAG, "recv prepare options: reconn interval = %d", v);
  }
  if (msg->read(v) != CAPS_SUCCESS) {
    goto msg_invalid;
  }
  if (v > 0) {
    popts.ping_interval = v;
    KLOGI(TAG, "recv prepare options: ping interval = %d", v);
  }
  if (msg->read(v) != CAPS_SUCCESS) {
    goto msg_invalid;
  }
  if (v > 0) {
    popts.no_resp_timeout = v;
    KLOGI(TAG, "recv prepare options: no resp timeout = %d", v);
  }
  if (!uri_parser.parse(uri.c_str())) {
    KLOGW(TAG, "invalid speech uri %s", uri.c_str());
    return;
  }
  popts.host = uri_parser.host;
  popts.port = uri_parser.port;
  popts.branch = uri_parser.path;
  speech_mutex.lock();
  speech->prepare(popts);
  speech_prepared = true;
  speech_cond.notify_one();
  speech_mutex.unlock();
  return;

msg_invalid:
  KLOGW(TAG, "invalid rokid.speech.prepare_options msg received");
}

void EventHandler::handle_speech_options(shared_ptr<Caps>& msg) {
  int32_t v;
  int32_t v2;
  shared_ptr<SpeechOptions> opts = SpeechOptions::new_instance();


  if (msg->read(v) != CAPS_SUCCESS) {
    goto msg_invalid;
  }
  if (v >= 0) {
    opts->set_lang((Lang)v);
    KLOGI(TAG, "recv speech options: lang = %d", v);
  }
  if (msg->read(v) != CAPS_SUCCESS) {
    goto msg_invalid;
  }
  if (v >= 0) {
    opts->set_codec((Codec)v);
    KLOGI(TAG, "recv speech options: codec = %d", v);
  }
  if (msg->read(v) != CAPS_SUCCESS) {
    goto msg_invalid;
  }
  if (msg->read(v2) != CAPS_SUCCESS) {
    goto msg_invalid;
  }
  if (v >= 0) {
    opts->set_vad_mode((VadMode)v, v2);
    KLOGI(TAG, "recv speech options: vad mode = %d, timeout = %d", v, v2);
  }
  if (msg->read(v) != CAPS_SUCCESS) {
    goto msg_invalid;
  }
  if (v >= 0) {
    opts->set_no_nlp(v);
    KLOGI(TAG, "recv speech options: no nlp = %d", v);
  }
  if (msg->read(v) != CAPS_SUCCESS) {
    goto msg_invalid;
  }
  if (v >= 0) {
    opts->set_no_intermediate_asr(v);
    KLOGI(TAG, "recv speech options: no intermediate asr = %d", v);
  }
  if (msg->read(v) != CAPS_SUCCESS) {
    goto msg_invalid;
  }
  if (v >= 0) {
    opts->set_vad_begin(v);
    KLOGI(TAG, "recv speech options: vad begin = %d", v);
  }
  speech->config(opts);
  return;

msg_invalid:
  KLOGW(TAG, "invalid rokid.speech.options msg received");
}

void EventHandler::handle_speech_stack(shared_ptr<Caps>& msg) {
  if (msg->read_string(speech_stack) != CAPS_SUCCESS) {
    KLOGW(TAG, "invalid rokid.speech.stack msg received");
    return;
  }
  KLOGI(TAG, "recv speech stack %s", speech_stack.c_str());
}

void EventHandler::handle_turen_start_voice(shared_ptr<Caps>& msg) {
  VoiceOptions vopts;
  int32_t v;
  int32_t turen_id;
  int32_t speech_id;

  open_pcm_file(lastest_speech_file);

  vopts.stack = speech_stack;
  KLOGI(TAG, "stack %s", speech_stack.c_str());
  if (msg->read_string(vopts.voice_trigger) != CAPS_SUCCESS) {
    goto msg_invalid;
  }
  KLOGI(TAG, "recv turen start_voice: voice trigger = %s", vopts.voice_trigger.c_str());
  if (msg->read(v) != CAPS_SUCCESS) {
    goto msg_invalid;
  }
  vopts.trigger_start = v;
  KLOGI(TAG, "recv turen start_voice: trigger start = %d", v);
  if (msg->read(v) != CAPS_SUCCESS) {
    goto msg_invalid;
  }
  vopts.trigger_length = v;
  KLOGI(TAG, "recv turen start_voice: trigger length = %d", v);
  if (msg->read(vopts.voice_power) != CAPS_SUCCESS) {
    goto msg_invalid;
  }
  KLOGI(TAG, "recv turen start_voice: voice power = %f", vopts.voice_power);
  if (msg->read(v) != CAPS_SUCCESS) {
    goto msg_invalid;
  }
  vopts.trigger_confirm_by_cloud = v;
  KLOGI(TAG, "recv turen start_voice: trigger confirm by cloud = %d", v);
  if (msg->read_string(vopts.voice_extra) != CAPS_SUCCESS) {
    goto msg_invalid;
  }
  KLOGI(TAG, "recv turen start_voice: voice extra = %s", vopts.voice_extra.c_str());
  if (msg->read(turen_id) != CAPS_SUCCESS) {
    goto msg_invalid;
  }
  KLOGI(TAG, "recv turen start_voice: turen id = %d", turen_id);
  // TODO: get skill options

  voice_mutex.lock();
  if (!pending_voices.empty()) {
    int32_t cid = pending_voices.back().second;
    KLOGI(TAG, "speech cancel, id = %d", cid);
    speech->cancel(cid);
  }
  speech_id = speech->start_voice(&vopts);
  KLOGI(TAG, "speech start voice, id = %d", speech_id);
  if (speech_id < 0) {
    post_error(nullptr, 200, turen_id);
    post_completed(turen_id);
  } else
    pending_voices.push_back(pair<int32_t, int32_t>(turen_id, speech_id));
  voice_mutex.unlock();
  return;

msg_invalid:
  KLOGW(TAG, "invalid rokid.turen.start_voice msg received");
}

void EventHandler::handle_turen_voice(shared_ptr<Caps>& msg) {
  string data;
  int32_t turen_id;
  int32_t speech_id;

  if (msg->read_binary(data) != CAPS_SUCCESS) {
    goto msg_invalid;
  }
  if (msg->read(turen_id) != CAPS_SUCCESS) {
    goto msg_invalid;
  }

  speech_id = get_speech_id(turen_id);
  if (speech_id < 0)
    post_completed(turen_id);
  else {
    write_pcm_file(data);
    speech->put_voice(speech_id, (const uint8_t*)data.data(), data.length());
  }
  return;

msg_invalid:
  KLOGW(TAG, "invalid rokid.turen.voice msg received");
}

void EventHandler::handle_turen_end_voice(shared_ptr<Caps>& msg) {
  int32_t turen_id;
  int32_t speech_id;

  if (msg->read(turen_id) != CAPS_SUCCESS) {
    goto msg_invalid;
  }
  speech_id = get_speech_id(turen_id);
  KLOGI(TAG, "turen end voice: turen id %d, speech id %d",
      turen_id, speech_id);
  if (speech_id > 0)
    speech->end_voice(speech_id);
  return;

msg_invalid:
  KLOGW(TAG, "invalid rokid.turen.end_voice msg received");
}

void EventHandler::handle_turen_sleep(shared_ptr<Caps>& msg) {
  voice_mutex.lock();
  if (!pending_voices.empty()) {
    speech->end_voice(pending_voices.back().second);
  }
  voice_mutex.unlock();
}

void EventHandler::handle_speech_put_text(shared_ptr<Caps>& msg) {
  string asr;
  string id;
  VoiceOptions vopts;
  int32_t sid;
  int32_t custom;
  list<TextReqInfo>::iterator it;

  if (msg->read_string(asr) != CAPS_SUCCESS) {
    goto msg_invalid;
  }
  if (msg->read_string(id) != CAPS_SUCCESS) {
    goto msg_invalid;
  }
  if (msg->read(custom) != CAPS_SUCCESS) {
    goto msg_invalid;
  }
  vopts.stack = speech_stack;
  // TODO: set skill options
  text_mutex.lock();
  sid = speech->put_text(asr.c_str(), &vopts);
  if (sid > 0) {
    it = pending_texts.emplace(pending_texts.end());
    (*it).msgid = id;
    (*it).speechid = sid;
    (*it).custom = custom;
    text_mutex.unlock();
  } else {
    text_mutex.unlock();
    post_error(id.c_str(), 200, custom);
  }
  return;

msg_invalid:
  KLOGW(TAG, "invalid rokid.speech.put_text s msg received");
}

void EventHandler::do_speech_poll() {
  SpeechResult result;
  shared_ptr<flora::Client> cli;
  shared_ptr<Caps> msg;
  string extid;
  int32_t custom;
  pair<int32_t, int32_t> front_req;

  unique_lock<mutex> locker(speech_mutex);
  if (speech_prepared == false)
    speech_cond.wait(locker);
  locker.unlock();

  while (true) {
    speech->poll(result);
    switch (result.type) {
      case SPEECH_RES_INTER:
        KLOGI(TAG, "speech poll INTER");
        if (result.asr.length() > 0) {
          post_inter_asr(result.asr, result.id);
        }
        if (result.extra.length() > 0) {
          post_extra(result.extra, result.id);
        }
        break;
      case SPEECH_RES_ASR_FINISH:
        KLOGI(TAG, "speech poll ASR_FINISH");
        post_inter_asr(result.asr, result.id);
        if (result.extra.length() > 0)
          post_extra(result.extra, result.id);
        post_completed(get_turen_id(result.id));
        break;
      case SPEECH_RES_END:
        KLOGI(TAG, "speech poll END");
        post_nlp(result.nlp, result.action, result.id);
        post_completed(get_turen_id(result.id));
        finish_voice_req(result.id);
        close_pcm_file();
        break;
      case SPEECH_RES_ERROR:
        KLOGI(TAG, "speech poll ERROR");
        close_pcm_file();
        post_error(result.err, result.id);
        post_completed(get_turen_id(result.id));
        finish_voice_req(result.id);
        break;
      case SPEECH_RES_CANCELLED:
        KLOGI(TAG, "speech poll CANCELLED");
        finish_voice_req(result.id);
        close_pcm_file();
        break;
    }
  }
}

void EventHandler::post_error(const char* suffix, int32_t err, int32_t id) {
  string name = "rokid.speech.error";
  shared_ptr<flora::Client> cli = flora_cli;
  shared_ptr<Caps> msg;

  if (cli.get() == nullptr)
    return;

  if (suffix) {
    name.append(".");
    name.append(suffix);
  }
  msg = Caps::new_instance();
  msg->write(err);
  msg->write(id);
  KLOGI(TAG, "speech post %s: err %d, id %d", name.c_str(), err, id);
  if (cli->post(name.c_str(), msg, FLORA_MSGTYPE_INSTANT)
      == FLORA_CLI_ECONN) {
    KLOGI(TAG, "notify keepalive thread: reconnect flora client");
    flora_disconnected();
  }
}

void EventHandler::post_error(int32_t err, int32_t speech_id) {
  unique_lock<mutex> voice_locker(voice_mutex);
  if (!pending_voices.empty()) {
    pair<int32_t, int32_t> front_req = pending_voices.back();
    if (front_req.second == speech_id) {
      post_error(nullptr, err, front_req.first);
      return;
    }
  }
  voice_locker.unlock();

  string msgid;
  int32_t custom;
  if (check_pending_texts(speech_id, msgid, custom)) {
    post_error(msgid.c_str(), err, custom);
  }
}

void EventHandler::post_completed(int32_t turen_id) {
  lock_guard<mutex> locker(completed_mutex);
  shared_ptr<flora::Client> cli = flora_cli;
  shared_ptr<Caps> msg;
  int32_t r;

  if (turen_id < 0)
    return;
  KLOGI(TAG, "post completed: turen id %d/%d", turen_id, lastest_completed_id);
  if (lastest_completed_id == turen_id)
    return;
  if (cli.get()) {
    msg = Caps::new_instance();
    msg->write(turen_id);
    r = cli->post("rokid.speech.completed", msg, FLORA_MSGTYPE_INSTANT);
    if (r == FLORA_CLI_SUCCESS)
      lastest_completed_id = turen_id;
    if (r == FLORA_CLI_ECONN) {
      KLOGI(TAG, "notify keepalive thread: reconnect flora client");
      flora_disconnected();
    }
  }
}

void EventHandler::post_inter_asr(const string& asr, int32_t speech_id) {
  shared_ptr<flora::Client> cli = flora_cli;
  shared_ptr<Caps> msg;
  int32_t turen_id;

  if (cli.get()) {
    turen_id = get_turen_id(speech_id);
    if (turen_id > 0) {
      KLOGI(TAG, "post inter asr %d-%s", turen_id, asr.c_str());
      msg = Caps::new_instance();
      msg->write(asr.c_str());
      msg->write(turen_id);
      if (cli->post("rokid.speech.inter_asr", msg, FLORA_MSGTYPE_INSTANT)
          == FLORA_CLI_ECONN) {
        KLOGI(TAG, "notify keepalive thread: reconnect flora client");
        flora_disconnected();
      }
    }
  }
}

void EventHandler::post_extra(const string& extra, int32_t speech_id) {
  shared_ptr<flora::Client> cli = flora_cli;
  shared_ptr<Caps> msg;
  int32_t turen_id;

  if (cli.get()) {
    turen_id = get_turen_id(speech_id);
    if (turen_id > 0) {
      KLOGI(TAG, "post extra %d-%s", turen_id, extra.c_str());
      msg = Caps::new_instance();
      msg->write(extra.c_str());
      msg->write(turen_id);
      if (cli->post("rokid.speech.extra", msg, FLORA_MSGTYPE_INSTANT)
          == FLORA_CLI_ECONN) {
        KLOGI(TAG, "notify keepalive thread: reconnect flora client");
        flora_disconnected();
      }
    }
  }
}

void EventHandler::post_final_asr(const string& asr, int32_t speech_id) {
  shared_ptr<flora::Client> cli = flora_cli;
  shared_ptr<Caps> msg;
  int32_t turen_id;

  if (cli.get()) {
    turen_id = get_turen_id(speech_id);
    if (turen_id > 0) {
      KLOGI(TAG, "post final asr %d-%s", turen_id, asr.c_str());
      msg = Caps::new_instance();
      msg->write(asr.c_str());
      msg->write(turen_id);
      if (cli->post("rokid.speech.final_asr", msg, FLORA_MSGTYPE_INSTANT)
          == FLORA_CLI_ECONN) {
        KLOGI(TAG, "notify keepalive thread: reconnect flora client");
        flora_disconnected();
      }
    }
  }
}

void EventHandler::post_nlp(const char* suffix, const string& nlp,
    const string& action, int32_t id) {
  shared_ptr<flora::Client> cli = flora_cli;
  shared_ptr<Caps> msg;
  string name = "rokid.speech.nlp";

  if (cli.get()) {
    if (suffix) {
      name.append(".");
      name.append(suffix);
    }
    KLOGI(TAG, "post %s %d-%s", name.c_str(), id, nlp.c_str());
    msg = Caps::new_instance();
    msg->write(nlp.c_str());
    msg->write(action.c_str());
    msg->write(id);
    if (cli->post(name.c_str(), msg, FLORA_MSGTYPE_INSTANT)
        == FLORA_CLI_ECONN) {
      KLOGI(TAG, "notify keepalive thread: reconnect flora client");
      flora_disconnected();
    }
  }
}

void EventHandler::post_nlp(const string& nlp, const string& action,
    int32_t speech_id) {
  unique_lock<mutex> voice_locker(voice_mutex);
  if (!pending_voices.empty()) {
    pair<int32_t, int32_t> front_req = pending_voices.back();
    if (front_req.second == speech_id) {
      post_nlp(nullptr, nlp, action, front_req.first);
      return;
    }
  }
  voice_locker.unlock();

  string msgid;
  int32_t custom;
  if (check_pending_texts(speech_id, msgid, custom)) {
    post_nlp(msgid.c_str(), nlp, action, custom);
  }
}

int32_t EventHandler::get_turen_id(int32_t speech_id) {
  lock_guard<mutex> locker(voice_mutex);
  if (pending_voices.empty())
    return -1;
  if (pending_voices.front().second == speech_id)
    return pending_voices.front().first;
  return -1;
}

int32_t EventHandler::get_speech_id(int32_t turen_id) {
  lock_guard<mutex> locker(voice_mutex);
  if (pending_voices.empty())
    return -1;
  if (pending_voices.front().first == turen_id)
    return pending_voices.front().second;
  return -1;
}

bool EventHandler::check_pending_texts(int32_t id, string& extid, int32_t& custom) {
  lock_guard<mutex> locker(text_mutex);
  bool ret = false;

  if (pending_texts.empty())
    return false;
  list<TextReqInfo>::iterator it;
  for (it = pending_texts.begin(); it != pending_texts.end(); ++it) {
    if ((*it).speechid == id) {
      extid = (*it).msgid;
      custom = (*it).custom;
      pending_texts.erase(it);
      ret = true;
      break;
    }
  }
  return ret;
}

void EventHandler::finish_voice_req(int32_t speech_id) {
  pair<int32_t, int32_t> req;

  if (speech_id < 0)
    return;
  voice_mutex.lock();
  if (pending_voices.empty()) {
    voice_mutex.unlock();
    KLOGW(TAG, "finish_voice_req: no pending voice request");
    return;
  }
  req = pending_voices.front();
  pending_voices.pop_front();
  voice_mutex.unlock();
  if (req.second != speech_id)
    KLOGW(TAG, "finish_voice_req: front req id not match: %d/%d", req.second, speech_id);
}

void EventHandler::flora_disconnected() {
  flora_cli.reset();
  reconn_mutex.lock();
  reconn_cond.notify_one();
  reconn_mutex.unlock();
}

void EventHandler::disconnected() {
  thread tmp([this]() { this->flora_disconnected(); });
  tmp.detach();
}

void EventHandler::flora_keepalive(CmdlineArgs& args) {
	shared_ptr<flora::Client> cli;
	int32_t r;
	unique_lock<mutex> locker(reconn_mutex);

	while (true) {
		r = flora::Client::connect(args.flora_uri.c_str(), this,
				args.flora_bufsize, cli);
		if (r != FLORA_CLI_SUCCESS) {
			KLOGI(TAG, "connect to flora service %s failed, retry after %u milliseconds",
					args.flora_uri.c_str(), args.flora_reconn_interval.count());
			reconn_cond.wait_for(locker, args.flora_reconn_interval);
		} else {
			KLOGI(TAG, "flora service %s connected", args.flora_uri.c_str());
			subscribe_events(cli);
      flora_cli = cli;
			reconn_cond.wait(locker);
		}
	}
}

void EventHandler::subscribe_events(shared_ptr<flora::Client>& cli) {
  EventHandlerMap::iterator it;

	for (it = handlers.begin(); it != handlers.end(); ++it) {
		cli->subscribe((*it).first.name.c_str(), (*it).first.type);
	}
}
