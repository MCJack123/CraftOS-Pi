extern "C" {
#include <lauxlib.h>
}
#include <circle/alloc.h>
#include "../event.hpp"

static CSoundBaseDevice *sound;
static bool sentEvent = false;

static void speaker_need_data(void* ud) {
    if (sentEvent || sound->GetQueueFramesAvail() > 1024) return;
    event_t ev;
    ev.type = EVENT_TYPE_SPEAKER_AUDIO_EMPTY;
    event_push(&ev);
    sentEvent = true;
}

void speaker_init(CSoundBaseDevice *m_Sound) {
    sound = m_Sound;
    sound->AllocateQueueFrames(131072 + 1024);
    sound->SetWriteFormat(SoundFormatUnsigned8, 2);
    sound->RegisterNeedDataCallback(speaker_need_data, NULL);
}

static int speaker_playNote(lua_State *L) {
    return 0;
}

static int speaker_playSound(lua_State *L) {
    return 0;
}

static int speaker_playAudio(lua_State *L) {
    luaL_checktype(L, 1, LUA_TTABLE);
    int nsamples = lua_rawlen(L, 1);
    if (nsamples == 0 || nsamples > sound->GetQueueSizeFrames()) {
        luaL_error(L, "Too many samples");
    }
    if (sound->GetQueueSizeFrames() - sound->GetQueueFramesAvail() < nsamples) {
        lua_pushboolean(L, false);
        return 1;
    }
    uint8_t* samples = (uint8_t*)malloc(nsamples * 2);
    for (int i = 0; i < nsamples; i++) {
        lua_rawgeti(L, 1, i+1);
        int n;
        if (!lua_isnumber(L, -1) || (n = lua_tointeger(L, -1)) < -128 || n > 127) {
            free(samples);
            return luaL_error(L, "bad argument #1 (sample #%d out of range)", i + 1);
        }
        samples[i*2] = samples[i*2+1] = n + 128;
        lua_pop(L, 1);
    }
    bool queued = sound->Write(samples, nsamples * 2) == nsamples * 2;
    if (!sound->IsActive()) sound->Start();
    sentEvent = false;
    free(samples);
    lua_pushboolean(L, queued);
    return 1;
}

static int speaker_stop(lua_State *L) {
    return 0;
}

extern "C" const luaL_Reg speaker_methods[] = {
    {"playNote", speaker_playNote},
    {"playSound", speaker_playSound},
    {"playAudio", speaker_playAudio},
    {"stop", speaker_stop},
    {NULL, NULL}
};
