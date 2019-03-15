#include "speech-service.h"
#include "rlog.h"
#include "uri.h"
#include <fcntl.h>
#include <unistd.h>

using namespace std;
using namespace rokid;
using namespace rokid::speech;
using namespace flora;

static int pcm_file = -1;
static int32_t pcm_speech_id = -1;
void open_pcm_file(const string &file, int32_t id) {
  if (file.length() > 0) {
    pcm_file = open(file.c_str(), O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if (pcm_file < 0) {
      KLOGW(TAG, "lastest speech file %s open failed", file.c_str());
    } else {
      pcm_speech_id = id;
    }
  }
}

void write_pcm_file(string &data, int32_t id) {
  if (pcm_file >= 0 && id == pcm_speech_id) {
    write(pcm_file, data.data(), data.length());
  }
}

void close_pcm_file(int32_t id) {
  if (pcm_file >= 0 && id == pcm_speech_id) {
    ::close(pcm_file);
    pcm_file = -1;
    pcm_speech_id = -1;
  }
}

SpeechService::SpeechService() {
  speech = Speech::new_instance();
#ifdef ASR2NLP_WORKAROUND
  speecht = Speech::new_instance();
#endif
}

void SpeechService::run(CmdlineArgs &args) {
  lastest_speech_file = args.lastest_speech_file;

  cmdline_args = &args;
  flora_agent.config(FLORA_AGENT_CONFIG_URI, args.flora_uri.c_str());
  flora_agent.config(FLORA_AGENT_CONFIG_BUFSIZE, args.flora_bufsize);
  flora_agent.subscribe(
      "rokid.speech.prepare_options",
      [this](const char *, shared_ptr<Caps> &msg, uint32_t type) {
        this->handle_speech_prepare_options(msg);
      });
  flora_agent.subscribe(
      "rokid.speech.options",
      [this](const char *, shared_ptr<Caps> &msg, uint32_t type) {
        this->handle_speech_options(msg);
      });
  flora_agent.subscribe(
      "rokid.speech.stack",
      [this](const char *, shared_ptr<Caps> &msg, uint32_t type) {
        this->handle_speech_stack(msg);
      });
  flora_agent.subscribe(
      "rokid.turen.start_voice",
      [this](const char *, shared_ptr<Caps> &msg, uint32_t type) {
        this->handle_turen_start_voice(msg);
      });
  flora_agent.subscribe(
      "rokid.turen.voice",
      [this](const char *, shared_ptr<Caps> &msg, uint32_t type) {
        this->handle_turen_voice(msg);
      });
  flora_agent.subscribe(
      "rokid.turen.end_voice",
      [this](const char *, shared_ptr<Caps> &msg, uint32_t type) {
        this->handle_turen_end_voice(msg);
      });
  flora_agent.subscribe(
      "rokid.turen.sleep",
      [this](const char *, shared_ptr<Caps> &msg, uint32_t type) {
        this->handle_turen_sleep(msg);
      });
  flora_agent.declare_method(
      "asr2nlp",
      [this](const char *, shared_ptr<Caps> &msg, shared_ptr<Reply> &reply) {
        this->handle_asr2nlp(msg, reply);
      });
  string skuri = args.flora_uri;
  skuri.append("0");
  skilloptions_agent.config(FLORA_AGENT_CONFIG_URI, skuri.c_str());
  skilloptions_agent.start();
  flora_agent.start(true);
}

void SpeechService::handle_speech_prepare_options(shared_ptr<Caps> &msg) {
  if (first_prepare) {
    if (!init_speech(msg)) {
      KLOGE(TAG,
            "speech sdk initialize failed, may be prepare options invalid");
    }
    first_prepare = false;
    std::thread speech_poll_thread([this]() { this->do_speech_poll(); });
    speech_poll_thread.detach();
#ifdef ASR2NLP_WORKAROUND
    std::thread speecht_poll_thread([this]() { this->do_speecht_poll(); });
    speecht_poll_thread.detach();
#endif
  } else {
    reconn_speech(msg);
  }
}

static bool caps_read_number(shared_ptr<Caps> &caps, void *res) {
  int32_t tp = caps->next_type();
  if (tp == CAPS_MEMBER_TYPE_DOUBLE) {
    double v;
    if (caps->read(v) != CAPS_SUCCESS)
      return false;
    reinterpret_cast<int32_t *>(res)[0] = v;
  } else if (tp == CAPS_MEMBER_TYPE_INTEGER) {
    if (caps->read(reinterpret_cast<int32_t *>(res)[0]) != CAPS_SUCCESS)
      return false;
  } else {
    return false;
  }
  return true;
}

static bool caps_to_prepare_options(shared_ptr<Caps> &data,
                                    PrepareOptions &popts) {
  string uri;
  if (data->read(uri) != CAPS_SUCCESS)
    return false;
  Uri urip;
  if (!urip.parse(uri.c_str()))
    return false;
  popts.host = urip.host;
  popts.port = urip.port;
  popts.branch = urip.path;
  if (data->read(popts.key) != CAPS_SUCCESS)
    return false;
  if (data->read(popts.device_type_id) != CAPS_SUCCESS)
    return false;
  if (data->read(popts.secret) != CAPS_SUCCESS)
    return false;
  if (data->read(popts.device_id) != CAPS_SUCCESS)
    return false;
  if (!caps_read_number(data, &popts.reconn_interval))
    return false;
  if (!caps_read_number(data, &popts.ping_interval))
    return false;
  if (!caps_read_number(data, &popts.no_resp_timeout))
    return false;
  caps_read_number(data, &popts.conn_duration);
  return true;
}

bool SpeechService::init_speech(shared_ptr<Caps> &data) {
  PrepareOptions popts;
  if (!caps_to_prepare_options(data, popts))
    return false;
  speech->prepare(popts);
#ifdef ASR2NLP_WORKAROUND
  speecht->prepare(popts);
#endif
  return true;
}

void SpeechService::reconn_speech(shared_ptr<Caps> &data) {
  PrepareOptions popts;
  if (!caps_to_prepare_options(data, popts))
    return;
  speech->prepare(popts);
  speech->reconn();
#ifdef ASR2NLP_WORKAROUND
  speecht->prepare(popts);
  speecht->reconn();
#endif
}

void SpeechService::handle_speech_options(shared_ptr<Caps> &msg) {
  if (!config_speech(msg)) {
    KLOGE(TAG, "speech sdk config failed, may be options invalid");
  }
}

bool SpeechService::config_speech(shared_ptr<Caps> &data) {
  shared_ptr<SpeechOptions> sopts = SpeechOptions::new_instance();
  int32_t v;
  if (!caps_read_number(data, &v))
    return false;
  sopts->set_lang((Lang)v);
  if (!caps_read_number(data, &v))
    return false;
  sopts->set_codec((Codec)v);
  if (!caps_read_number(data, &v))
    return false;
  uint32_t u;
  if (!caps_read_number(data, &u))
    return false;
  sopts->set_vad_mode((VadMode)v, u);
  if (!caps_read_number(data, &v))
    return false;
  sopts->set_no_nlp(v != 0);
  if (!caps_read_number(data, &v))
    return false;
  sopts->set_no_intermediate_asr(v != 0);
  if (!caps_read_number(data, &u))
    return false;
  sopts->set_vad_begin(u);
  if (!caps_read_number(data, &u))
    return false;
  sopts->set_voice_fragment(u);
  speech->config(sopts);
  return true;
}

void SpeechService::handle_speech_stack(shared_ptr<Caps> &msg) {
  if (msg->read(stack) != CAPS_SUCCESS) {
    KLOGE(TAG, "invalid rokid.speech.stack");
    return;
  }
}

void SpeechService::handle_turen_start_voice(shared_ptr<Caps> &msg) {
  VoiceOptions vopts;
  int32_t turen_id;
  int32_t speech_id;

  vopts.stack = stack;
  KLOGI(TAG, "stack %s", stack.c_str());
  if (msg->read_string(vopts.voice_trigger) != CAPS_SUCCESS) {
    goto msg_invalid;
  }
  KLOGI(TAG, "recv turen start_voice: voice trigger = %s",
        vopts.voice_trigger.c_str());
  if (msg->read(vopts.trigger_start) != CAPS_SUCCESS) {
    goto msg_invalid;
  }
  KLOGI(TAG, "recv turen start_voice: trigger start = %d", vopts.trigger_start);
  if (msg->read(vopts.trigger_length) != CAPS_SUCCESS) {
    goto msg_invalid;
  }
  KLOGI(TAG, "recv turen start_voice: trigger length = %d",
        vopts.trigger_length);
  if (msg->read(vopts.voice_power) != CAPS_SUCCESS) {
    goto msg_invalid;
  }
  KLOGI(TAG, "recv turen start_voice: voice power = %f", vopts.voice_power);
  if (msg->read(vopts.trigger_confirm_by_cloud) != CAPS_SUCCESS) {
    goto msg_invalid;
  }
  KLOGI(TAG, "recv turen start_voice: trigger confirm by cloud = %d",
        vopts.trigger_confirm_by_cloud);
  if (msg->read_string(vopts.voice_extra) != CAPS_SUCCESS) {
    goto msg_invalid;
  }
  KLOGI(TAG, "recv turen start_voice: voice extra = %s",
        vopts.voice_extra.c_str());
  if (msg->read(turen_id) != CAPS_SUCCESS) {
    goto msg_invalid;
  }
  KLOGI(TAG, "recv turen start_voice: turen id = %d", turen_id);
  get_skilloptions(vopts.skill_options);
  KLOGI(TAG, "skilloptions: %s", vopts.skill_options.c_str());

  voice_mutex.lock();
  if (!pending_voices.empty()) {
    int32_t cid = pending_voices.back().second;
    KLOGI(TAG, "speech cancel, id = %d", cid);
    speech->cancel(cid);
    close_pcm_file(cid);
  }
  speech_id = speech->start_voice(&vopts);
  open_pcm_file(lastest_speech_file, speech_id);
  KLOGI(TAG, "speech start voice, id = %d", speech_id);
  if (speech_id < 0) {
    post_error(200, turen_id);
    post_completed(turen_id);
  } else
    pending_voices.push_back(pair<int32_t, int32_t>(turen_id, speech_id));
  voice_mutex.unlock();
  return;

msg_invalid:
  KLOGW(TAG, "invalid rokid.turen.start_voice msg received");
}

void SpeechService::handle_turen_voice(shared_ptr<Caps> &msg) {
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
  if (speech_id < 0) {
    post_completed(turen_id);
  } else {
    write_pcm_file(data, speech_id);
    speech->put_voice(speech_id, (const uint8_t *)data.data(), data.length());
  }
  return;

msg_invalid:
  KLOGW(TAG, "invalid rokid.turen.voice msg received");
}

void SpeechService::handle_turen_end_voice(shared_ptr<Caps> &msg) {
  int32_t turen_id;
  int32_t speech_id;

  if (msg->read(turen_id) != CAPS_SUCCESS) {
    goto msg_invalid;
  }
  speech_id = get_speech_id(turen_id);
  KLOGI(TAG, "turen end voice: turen id %d, speech id %d", turen_id, speech_id);
  if (speech_id > 0)
    speech->end_voice(speech_id);
  return;

msg_invalid:
  KLOGW(TAG, "invalid rokid.turen.end_voice msg received");
}

void SpeechService::handle_turen_sleep(shared_ptr<Caps> &msg) {
  voice_mutex.lock();
  if (!pending_voices.empty()) {
    speech->end_voice(pending_voices.back().second);
  }
  voice_mutex.unlock();
}

void SpeechService::handle_asr2nlp(shared_ptr<Caps> &msg,
                                   shared_ptr<Reply> &reply) {
  string asr;
  VoiceOptions vopts;
  int32_t sid;
  list<TextReqInfo>::iterator it;

  if (msg->read_string(asr) != CAPS_SUCCESS) {
    goto msg_invalid;
  }
  if (msg->read_string(vopts.skill_options) != CAPS_SUCCESS) {
    goto msg_invalid;
  }
  vopts.stack = stack;
  KLOGI(TAG, "put text %s with skill options %s", asr.c_str(),
        vopts.skill_options.c_str());
  text_mutex.lock();
#ifdef ASR2NLP_WORKAROUND
  sid = speecht->put_text(asr.c_str(), &vopts);
#else
  sid = speech->put_text(asr.c_str(), &vopts);
#endif
  if (sid > 0) {
    it = pending_texts.emplace(pending_texts.end());
    (*it).speechid = sid;
    (*it).reply = reply;
    text_mutex.unlock();
  } else {
    text_mutex.unlock();
    reply->end(SPEECH_SERVICE_RPC_ERROR_NOT_READY);
  }
  return;

msg_invalid:
  KLOGW(TAG, "invalid rokid.speech.put_text msg received");
  reply->end(SPEECH_SERVICE_RPC_ERROR_INVALID_PARAM);
}

void SpeechService::do_speech_poll() {
  SpeechResult result;
  int32_t turen_id;

  while (true) {
    speech->poll(result);
    switch (result.type) {
    case SPEECH_RES_START:
      break;
    case SPEECH_RES_INTER:
      KLOGI(TAG, "speech poll INTER");
      turen_id = get_turen_id(result.id);
      if (result.asr.length() > 0) {
        post_inter_asr(result.asr, turen_id);
      }
      if (result.extra.length() > 0) {
        post_extra(result.extra, turen_id);
      }
      break;
    case SPEECH_RES_ASR_FINISH:
      KLOGI(TAG, "speech poll ASR_FINISH");
      turen_id = get_turen_id(result.id);
      post_final_asr(result.asr, turen_id);
      if (result.extra.length() > 0)
        post_extra(result.extra, turen_id);
      post_completed(turen_id);
      break;
    case SPEECH_RES_END:
      KLOGI(TAG, "speech poll END");
#ifdef ASR2NLP_WORKAROUND
      turen_id = get_turen_id(result.id);
      post_nlp(result.nlp, result.action, turen_id);
      post_completed(turen_id);
      finish_voice_req(result.id);
      close_pcm_file(result.id);
#else
      if (!end_pending_texts(result.id, result.nlp, result.action)) {
        turen_id = get_turen_id(result.id);
        post_nlp(result.nlp, result.action, turen_id);
        post_completed(turen_id);
        finish_voice_req(result.id);
        close_pcm_file(result.id);
      }
#endif
      break;
    case SPEECH_RES_ERROR:
      KLOGI(TAG, "speech poll ERROR");
#ifdef ASR2NLP_WORKAROUND
      turen_id = get_turen_id(result.id);
      close_pcm_file(result.id);
      post_error(result.err, turen_id);
      post_completed(turen_id);
      finish_voice_req(result.id);
#else
      if (!end_pending_texts(result.id, result.err)) {
        turen_id = get_turen_id(result.id);
        close_pcm_file(result.id);
        post_error(result.err, turen_id);
        post_completed(turen_id);
        finish_voice_req(result.id);
      }
#endif
      break;
    case SPEECH_RES_CANCELLED:
      KLOGI(TAG, "speech poll CANCELLED");
      finish_voice_req(result.id);
      close_pcm_file(result.id);
      break;
    }
  }
}

#ifdef ASR2NLP_WORKAROUND
void SpeechService::do_speecht_poll() {
  SpeechResult result;
  int32_t turen_id;

  while (true) {
    speecht->poll(result);
    switch (result.type) {
    case SPEECH_RES_END:
      end_pending_texts(result.id, result.nlp, result.action);
      break;
    case SPEECH_RES_ERROR:
      end_pending_texts(result.id, result.err);
      break;
    }
  }
}
#endif

void SpeechService::post_nlp(const string &nlp, const string &action,
                             int32_t id) {
  if (id < 0)
    return;
  shared_ptr<Caps> msg;
  string name = "rokid.speech.nlp";
  int32_t r;
  msg = Caps::new_instance();
  msg->write(nlp.c_str());
  msg->write(action.c_str());
  msg->write(id);
  KLOGI(TAG, "post %s %d-%s", name.c_str(), id, nlp.c_str());
  if ((r = flora_agent.post(name.c_str(), msg)) != FLORA_CLI_SUCCESS) {
    KLOGW(TAG, "flora msg post failed: %d", r);
  }
}

void SpeechService::post_error(int32_t err, int32_t id) {
  string name = "rokid.speech.error";
  shared_ptr<Caps> msg;
  int32_t r;
  msg = Caps::new_instance();
  msg->write(err);
  msg->write(id);
  KLOGI(TAG, "speech post %s: err %d, id %d", name.c_str(), err, id);
  if ((r = flora_agent.post(name.c_str(), msg)) != FLORA_CLI_SUCCESS) {
    KLOGW(TAG, "flora msg post failed: %d", r);
  }
}

void SpeechService::post_inter_asr(const string &asr, int32_t turen_id) {
  shared_ptr<Caps> msg;

  if (turen_id > 0) {
    KLOGI(TAG, "post inter asr %d-%s", turen_id, asr.c_str());
    msg = Caps::new_instance();
    msg->write(asr.c_str());
    msg->write(turen_id);
    int32_t r;
    if ((r = flora_agent.post("rokid.speech.inter_asr", msg)) !=
        FLORA_CLI_SUCCESS) {
      KLOGW(TAG, "flora msg post failed: %d", r);
    }
  }
}

void SpeechService::post_extra(const string &extra, int32_t turen_id) {
  shared_ptr<Caps> msg;

  if (turen_id > 0) {
    KLOGI(TAG, "post extra %d-%s", turen_id, extra.c_str());
    msg = Caps::new_instance();
    msg->write(extra.c_str());
    msg->write(turen_id);
    int32_t r;
    if ((r = flora_agent.post("rokid.speech.extra", msg)) !=
        FLORA_CLI_SUCCESS) {
      KLOGW(TAG, "flora msg post failed: %d", r);
    }
  }
}

void SpeechService::post_final_asr(const string &asr, int32_t turen_id) {
  shared_ptr<Caps> msg;

  if (turen_id > 0) {
    KLOGI(TAG, "post final asr %d-%s", turen_id, asr.c_str());
    msg = Caps::new_instance();
    msg->write(asr.c_str());
    msg->write(turen_id);
    int32_t r;
    if ((r = flora_agent.post("rokid.speech.final_asr", msg)) !=
        FLORA_CLI_SUCCESS) {
      KLOGW(TAG, "flora msg post failed: %d", r);
    }
  }
}

void SpeechService::post_completed(int32_t turen_id) {
  lock_guard<mutex> locker(completed_mutex);
  shared_ptr<Caps> msg;
  int32_t r;

  if (turen_id < 0)
    return;
  KLOGI(TAG, "post completed: turen id %d/%d", turen_id, lastest_completed_id);
  if (lastest_completed_id == turen_id)
    return;
  msg = Caps::new_instance();
  msg->write(turen_id);
  r = flora_agent.post("rokid.speech.completed", msg);
  if (r == FLORA_CLI_SUCCESS)
    lastest_completed_id = turen_id;
  else {
    KLOGW(TAG, "flora msg post failed: %d", r);
  }
}

void SpeechService::finish_voice_req(int32_t speech_id) {
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
    KLOGW(TAG, "finish_voice_req: front req id not match: %d/%d", req.second,
          speech_id);
}

int32_t SpeechService::get_turen_id(int32_t speech_id) {
  lock_guard<mutex> locker(voice_mutex);
  if (pending_voices.empty())
    return -1;
  if (pending_voices.front().second == speech_id)
    return pending_voices.front().first;
  return -1;
}

int32_t SpeechService::get_speech_id(int32_t turen_id) {
  lock_guard<mutex> locker(voice_mutex);
  if (pending_voices.empty())
    return -1;
  if (pending_voices.back().first == turen_id)
    return pending_voices.back().second;
  return -1;
}

bool SpeechService::end_pending_texts(int32_t speech_id, const string &nlp,
                                      const string &action) {
  TextReqInfo info;
  if (!pop_text_req(speech_id, info))
    return false;
  shared_ptr<Caps> data = Caps::new_instance();
  data->write(nlp);
  data->write(action);
  info.reply->end(FLORA_CLI_SUCCESS, data);
  return true;
}

bool SpeechService::end_pending_texts(int32_t speech_id, int32_t err) {
  TextReqInfo info;
  if (!pop_text_req(speech_id, info))
    return false;
  shared_ptr<Caps> data = Caps::new_instance();
  data->write(err);
  info.reply->end(SPEECH_SERVICE_RPC_ERROR_CLOUD, data);
  return true;
}

bool SpeechService::pop_text_req(int32_t speech_id, TextReqInfo &res) {
  lock_guard<mutex> locker(text_mutex);
  if (pending_texts.empty())
    return false;
  res = pending_texts.front();
  if (res.speechid == speech_id) {
    pending_texts.pop_front();
    return true;
  }
  KLOGW(TAG, "pending text request invalid, speech id %d, excepted %d",
        res.speechid, speech_id);
  pending_texts.clear();
  return false;
}

void SpeechService::get_skilloptions(std::string &res) {
  shared_ptr<Caps> empty;
  Response resp;
  int32_t r = skilloptions_agent.call(
      "rokid.skilloptions", empty, cmdline_args->skilloptions_provider.c_str(),
      resp, 1000);
  if (r == FLORA_CLI_SUCCESS) {
    if (resp.data == nullptr) {
      r = -401;
    } else if (resp.data->read(res) != CAPS_SUCCESS) {
      r = -401;
    }
  }
  if (r)
    KLOGW(TAG, "get skill options failed: %d", r);
}
