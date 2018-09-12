#include <thread>
#include "event-handler.h"
#include "defs.h"
#include "rlog.h"
#include "speech.h"
#include "uri.h"

// debug
// #include <sys/types.h>
// #include <sys/stat.h>
// #include <fcntl.h>
// #include <unistd.h>

#define DEFAULT_SPEECH_URI "wss://apigwws.open.rokid.com:443/api"

using namespace std;
using namespace rokid;
using namespace rokid::speech;

typedef struct {
  const char* event;
  EventHandleFunc handler;
} NamedEventHandler;

static NamedEventHandler named_handler[] = {
  { "rokid.speech.prepare_options", &EventHandler::handle_speech_prepare_options },
  { "rokid.speech.options", &EventHandler::handle_speech_options },
  { "rokid.speech.stack", &EventHandler::handle_speech_stack },
  { "rokid.turen.start_voice", &EventHandler::handle_turen_start_voice },
  { "rokid.turen.voice", &EventHandler::handle_turen_voice },
  { "rokid.turen.sleep", &EventHandler::handle_turen_sleep },
  { "rokid.speech.put_text", &EventHandler::handle_speech_put_text},
};

// debug
/**
static int pcm_file = -1;
void open_pcm_file() {
  pcm_file = open("/data/speech-service.pcm", O_CREAT | O_WRONLY);
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
*/

void EventHandler::init(CmdlineArgs& args) {
  int32_t count = sizeof(named_handler) / sizeof(NamedEventHandler);
  int32_t i;

  for (i = 0; i < count; ++i) {
    handlers[named_handler[i].event] = named_handler[i].handler;
  }

  speech = Speech::new_instance();
  std::thread speech_poll_thread([this]() { this->do_speech_poll(); });
  speech_poll_thread.detach();
}

void EventHandler::set_flora_client(shared_ptr<flora::Client>& cli) {
  flora_cli = std::move(cli);
}

void EventHandler::recv_post(const char* name, uint32_t msgtype,
    shared_ptr<Caps>& msg) {
  EventHandlerMap::iterator it = handlers.find(name);
  if (it == handlers.end()) {
    KLOGW(TAG, "event handler for %s not found", name);
    return;
  }
  EventHandleFunc handler = it->second;
  (this->*handler)(msgtype, msg);
}

void EventHandler::handle_speech_prepare_options(uint32_t msgtype, shared_ptr<Caps>& msg) {
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
  KLOGW(TAG, "invalid %s msg received", named_handler[0].event);
}

void EventHandler::handle_speech_options(uint32_t msgtype, shared_ptr<Caps>& msg) {
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
  KLOGW(TAG, "invalid %s msg received", named_handler[1].event);
}

void EventHandler::handle_speech_stack(uint32_t msgtype, shared_ptr<Caps>& msg) {
  if (msg->read_string(speech_stack) != CAPS_SUCCESS) {
    KLOGW(TAG, "invalid %s msg received", named_handler[2].event);
    return;
  }
  KLOGI(TAG, "recv speech stack %s", speech_stack.c_str());
}

void EventHandler::handle_turen_start_voice(uint32_t msgtype, shared_ptr<Caps>& msg) {
  VoiceOptions vopts;
  int32_t v;

  vopts.stack = speech_stack;
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
  cancelled_turen_id = turen_id;
  if (msg->read(turen_id) != CAPS_SUCCESS) {
    goto msg_invalid;
  }
  KLOGI(TAG, "recv turen start_voice: turen id = %d", turen_id);
  // TODO: get skill options

  if (speech_id > 0) {
    KLOGI(TAG, "speech cancel, id = %d", speech_id);
    speech->cancel(speech_id);
  }
  speech_id = speech->start_voice(&vopts);
  KLOGI(TAG, "speech start voice, id = %d", speech_id);
  if (speech_id < 0)
    post_error(200, -1);
  return;

msg_invalid:
  KLOGW(TAG, "invalid %s msg received", named_handler[3].event);
}

void EventHandler::handle_turen_voice(uint32_t msgtype, shared_ptr<Caps>& msg) {
  string data;
  if (msg->read_binary(data) != CAPS_SUCCESS) {
    goto msg_invalid;
  }
  if (speech_id > 0) {
    speech->put_voice(speech_id, (const uint8_t*)data.data(), data.length());
  } else
    KLOGI(TAG, "turen voice discard: invalid speech id %d", speech_id);
  return;

msg_invalid:
  KLOGW(TAG, "invalid %s msg received", named_handler[4].event);
}

void EventHandler::handle_turen_sleep(uint32_t msgtype, shared_ptr<Caps>& msg) {
  if (speech_id > 0) {
    speech->end_voice(speech_id);
    speech_id = -1;
  }
}

void EventHandler::handle_speech_put_text(uint32_t msgtype, shared_ptr<Caps>& msg) {
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
  it = pending_texts.emplace(pending_texts.end());
  (*it).msgid = id;
  (*it).speechid = sid;
  (*it).custom = custom;
  text_mutex.unlock();

msg_invalid:
  KLOGW(TAG, "invalid %s msg received", named_handler[6].event);
}

void EventHandler::do_speech_poll() {
  SpeechResult result;
  shared_ptr<flora::Client> cli;
  shared_ptr<Caps> msg;
  string extid;
  int32_t custom;

  unique_lock<mutex> locker(speech_mutex);
  if (speech_prepared == false)
    speech_cond.wait(locker);
  locker.unlock();

  while (true) {
    speech->poll(result);
    switch (result.type) {
      case SPEECH_RES_INTER:
        KLOGI(TAG, "speech poll INTER");
        cli = flora_cli;
        if (cli.get()) {
          if (result.asr.length() > 0) {
            msg = Caps::new_instance();
            msg->write(result.asr.c_str());
            msg->write(turen_id);
            KLOGI(TAG, "speech post rokid.speech.inter_asr [%d]%s",
                turen_id, result.asr.c_str());
            if (cli->post("rokid.speech.inter_asr", msg, FLORA_MSGTYPE_INSTANT)
                == FLORA_CLI_ECONN) {
              KLOGI(TAG, "notify keepalive thread: reconnect flora client");
              flora_disconnected();
            }
          }
          if (result.extra.length() > 0) {
            msg = Caps::new_instance();
            msg->write(result.extra.c_str());
            msg->write(turen_id);
            KLOGI(TAG, "speech post rokid.speech.extra [%d]%s",
                turen_id, result.extra.c_str());
            if (cli->post("rokid.speech.extra", msg, FLORA_MSGTYPE_INSTANT)
                == FLORA_CLI_ECONN) {
              KLOGI(TAG, "notify keepalive thread: reconnect flora client");
              flora_disconnected();
            }
          }
        }
        break;
      case SPEECH_RES_ASR_FINISH:
        KLOGI(TAG, "speech poll ASR_FINISH");
        cli = flora_cli;
        if (cli.get()) {
          msg = Caps::new_instance();
          msg->write(result.asr.c_str());
          msg->write(turen_id);
          KLOGI(TAG, "speech post rokid.speech.final_asr [%d]%s",
              turen_id, result.asr.c_str());
          if (cli->post("rokid.speech.final_asr", msg, FLORA_MSGTYPE_INSTANT)
              == FLORA_CLI_ECONN) {
            KLOGI(TAG, "notify keepalive thread: reconnect flora client");
            flora_disconnected();
          }
        }
        break;
      case SPEECH_RES_END:
        KLOGI(TAG, "speech poll END");
        cli = flora_cli;
        if (cli.get()) {
          string msgname = "rokid.speech.nlp";
          if (check_pending_texts(result.id, extid, custom)) {
            msgname.append(".");
            msgname.append(extid);
          }
          msg = Caps::new_instance();
          msg->write(result.nlp.c_str());
          msg->write(result.action.c_str());
          msg->write(custom);
          if (cli->post(msgname.c_str(), msg, FLORA_MSGTYPE_INSTANT)
              == FLORA_CLI_ECONN) {
            KLOGI(TAG, "notify keepalive thread: reconnect flora client");
            flora_disconnected();
          }
        }
        break;
      case SPEECH_RES_ERROR:
        KLOGI(TAG, "speech poll ERROR");
        post_error((int32_t)result.err, result.id);
        break;
      case SPEECH_RES_CANCELLED:
        KLOGI(TAG, "speech poll CANCELLED");
        cli = flora_cli;
        if (cli.get()) {
          string msgname = "rokid.speech.cancel";
          if (check_pending_texts(result.id, extid, custom)) {
            msgname.append(".");
            msgname.append(extid);
          }
          msg = Caps::new_instance();
          msg->write(cancelled_turen_id);
          msg->write(custom);
          KLOGI(TAG, "speech post rokid.speech.cancel %d",
              cancelled_turen_id);
          if (cli->post(msgname.c_str(), msg, FLORA_MSGTYPE_INSTANT)
              == FLORA_CLI_ECONN) {
            KLOGI(TAG, "notify keepalive thread: reconnect flora client");
            flora_disconnected();
          }
        }
        break;
    }
  }
}

void EventHandler::post_error(int32_t err, int32_t id) {
  shared_ptr<flora::Client> cli;
  shared_ptr<Caps> msg;
  string extid;
  int32_t custom;

  cli = flora_cli;
  if (cli.get()) {
    string msgname = "rokid.speech.error";
    if (check_pending_texts(id, extid, custom)) {
      msgname.append(".");
      msgname.append(extid);
    }
    msg = Caps::new_instance();
    msg->write(err);
    msg->write(turen_id);
    msg->write(custom);
    KLOGI(TAG, "speech post rokid.speech.error [%d]%d",
        turen_id, err);
    if (cli->post(msgname.c_str(), msg, FLORA_MSGTYPE_INSTANT)
        == FLORA_CLI_ECONN) {
      KLOGI(TAG, "notify keepalive thread: reconnect flora client");
      flora_disconnected();
    }
  }
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
