/*
 * AstroMenace audio backend for AmigaChrome.
 * Replaces the upstream OpenAL/Vorbis implementation with the OpenGPU
 * SDL2_mixer satellite. Ogg/Vorbis decoding therefore follows the normal
 * OpenGPU media.decode -> CPU fallback ladder and output follows SDL2's
 * Amiga audio backend.
 */
#include "audio.h"
#include "../vfs/vfs.h"
#include "../math/math.h"
#include "SDL.h"
#include "SDL_mixer.h"

#include <cmath>
#include <deque>
#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>

namespace viewizard {
namespace {

bool AudioReady = false;
float ListenerX = 0.0f;
float ListenerY = 0.0f;
float ListenerZ = 0.0f;

struct BufferEntry {
    unsigned id;
    Mix_Chunk *chunk;
};

struct SoundState {
    std::string name;
    int channel;
    float localVolume;
    float globalVolume;
    bool allowedStop;
    bool relative;
    int attenuationType;
    float x, y, z;
    uint32_t lastTick;
    uint32_t destroyPeriod;
    uint32_t destroyTicks;
};

/* A music file being decoded on the decode thread. Decoding a whole track
   (13-31 MB of PCM) takes seconds on a 68040, so it never runs on the game's
   thread: the file is read here, decoded there, and played once ready. */
struct DecodeJob {
    std::unique_ptr<cFILE> file;
    Mix_Chunk *chunk = nullptr;
    bool done = false;          // under DecodeLock
    bool cancelled = false;     // under DecodeLock
};

struct MusicState {
    Mix_Chunk *chunk;
    int channel;
    std::shared_ptr<DecodeJob> pending;     // the track being decoded
    std::shared_ptr<DecodeJob> pendingLoop; // its loop part, decoded meanwhile
    int pendingLoops;
    float localVolume;
    float globalVolume;
    bool looped;
    std::string loopPart;
    bool fadeIn;
    bool fadeOut;
    float fadeStart;
    float fadeEnd;
    uint32_t fadeTicks;
    uint32_t fadePeriod;
    uint32_t lastTick;
};

std::unordered_map<std::string, BufferEntry> SoundBuffers;
std::unordered_map<unsigned, SoundState> Sounds;
std::unordered_map<std::string, MusicState> Music;
unsigned NextBufferId = 1;
unsigned NextSoundId = 1;

/* Channels 0..MusicChannels-1 are reserved for music (Mix_ReserveChannels),
   so a sound never takes a music channel, and a stale channel number kept by
   a finished sound can never halt or re-volume the music. Two themes overlap
   during a cross-fade; four leaves room to spare. */
constexpr int MusicChannels = 4;

SDL_Thread *DecodeThread = nullptr;
SDL_mutex *DecodeLock = nullptr;
SDL_cond *DecodeWake = nullptr;
std::deque<std::shared_ptr<DecodeJob>> DecodeQueue;
bool DecodeStop = false;

static int mixVolume(float v)
{
    if (v < 0.0f) v = 0.0f;
    if (v > 1.0f) v = 1.0f;
    return static_cast<int>(v * static_cast<float>(MIX_MAX_VOLUME));
}

static Mix_Chunk *loadChunk(const std::string &name)
{
    std::unique_ptr<cFILE> file = vw_fopen(name);
    if (!file || file->GetSize() <= 0 || file->GetData() == nullptr)
        return nullptr;

    SDL_RWops *rw = SDL_RWFromConstMem(file->GetData(), static_cast<int>(file->GetSize()));
    if (!rw)
        return nullptr;

    /* Mix_LoadWAV_RW is intentionally used for both WAV and Ogg. OpenGPU's
       SDL2_mixer satellite advertises Ogg through this path and can service
       it via media.decode before falling back to its bundled stb_vorbis. */
    return Mix_LoadWAV_RW(rw, 1);
}

static int decodeMain(void *)
{
    SDL_LockMutex(DecodeLock);
    for (;;) {
        while (DecodeQueue.empty() && !DecodeStop)
            SDL_CondWait(DecodeWake, DecodeLock);
        if (DecodeStop)
            break;
        std::shared_ptr<DecodeJob> job = DecodeQueue.front();
        DecodeQueue.pop_front();
        if (job->cancelled)
            continue;
        SDL_UnlockMutex(DecodeLock);
        Mix_Chunk *chunk = nullptr;
        SDL_RWops *rw = SDL_RWFromConstMem(job->file->GetData(), static_cast<int>(job->file->GetSize()));
        if (rw)
            chunk = Mix_LoadWAV_RW(rw, 1);
        SDL_LockMutex(DecodeLock);
        if (job->cancelled) {
            if (chunk)
                Mix_FreeChunk(chunk);
        } else {
            job->chunk = chunk;
        }
        job->file.reset();
        job->done = true;
    }
    SDL_UnlockMutex(DecodeLock);
    return 0;
}

/* Reads the file here (the VFS stays on the game's thread) and queues it. */
static std::shared_ptr<DecodeJob> queueDecode(const std::string &name)
{
    std::unique_ptr<cFILE> file = vw_fopen(name);
    if (!file || file->GetSize() <= 0 || file->GetData() == nullptr)
        return nullptr;
    auto job = std::make_shared<DecodeJob>();
    if (!DecodeThread) {
        /* no thread could be made: decode here, as before */
        SDL_RWops *rw = SDL_RWFromConstMem(file->GetData(), static_cast<int>(file->GetSize()));
        job->chunk = rw ? Mix_LoadWAV_RW(rw, 1) : nullptr;
        job->done = true;
        return job;
    }
    job->file = std::move(file);
    SDL_LockMutex(DecodeLock);
    DecodeQueue.push_back(job);
    SDL_CondSignal(DecodeWake);
    SDL_UnlockMutex(DecodeLock);
    return job;
}

/* Takes a finished decode: 1 with the chunk (may be null: it failed), 0 while
   it is still running. */
static int collectDecode(const std::shared_ptr<DecodeJob> &job, Mix_Chunk *&chunk)
{
    if (DecodeLock) SDL_LockMutex(DecodeLock);
    const bool done = job->done;
    chunk = done ? job->chunk : nullptr;
    if (done) job->chunk = nullptr;
    if (DecodeLock) SDL_UnlockMutex(DecodeLock);
    return done ? 1 : 0;
}

static void cancelDecode(std::shared_ptr<DecodeJob> &job)
{
    if (!job)
        return;
    if (DecodeLock) SDL_LockMutex(DecodeLock);
    job->cancelled = true;
    if (job->chunk) {
        Mix_FreeChunk(job->chunk);
        job->chunk = nullptr;
    }
    if (DecodeLock) SDL_UnlockMutex(DecodeLock);
    job.reset();
}

static void stopDecodeThread()
{
    if (!DecodeThread)
        return;
    SDL_LockMutex(DecodeLock);
    DecodeStop = true;
    for (auto &job : DecodeQueue)
        job->cancelled = true;
    DecodeQueue.clear();
    SDL_CondSignal(DecodeWake);
    SDL_UnlockMutex(DecodeLock);
    SDL_WaitThread(DecodeThread, nullptr);
    DecodeThread = nullptr;
    SDL_DestroyCond(DecodeWake);
    SDL_DestroyMutex(DecodeLock);
    DecodeWake = nullptr;
    DecodeLock = nullptr;
    DecodeStop = false;
}

static BufferEntry *bufferFor(const std::string &name)
{
    auto it = SoundBuffers.find(name);
    if (it != SoundBuffers.end())
        return &it->second;

    Mix_Chunk *chunk = loadChunk(name);
    if (!chunk)
        return nullptr;

    BufferEntry entry{NextBufferId++, chunk};
    auto result = SoundBuffers.emplace(name, entry);
    return &result.first->second;
}

static void applySpatial(SoundState &s)
{
    if (s.channel < 0)
        return;

    float dx = s.relative ? s.x : (s.x - ListenerX);
    float dy = s.relative ? s.y : (s.y - ListenerY);
    float dz = s.relative ? s.z : (s.z - ListenerZ);
    float distance = std::sqrt(dx * dx + dy * dy + dz * dz);
    float maxDistance = s.attenuationType == 1 ? 250.0f :
                        (s.attenuationType == 2 ? 600.0f : 1200.0f);
    float attenuation = 1.0f - distance / maxDistance;
    if (attenuation < 0.08f) attenuation = 0.08f;
    if (attenuation > 1.0f) attenuation = 1.0f;

    Mix_Volume(s.channel, mixVolume(s.localVolume * s.globalVolume * attenuation));

    float pan = dx / 220.0f;
    if (pan < -1.0f) pan = -1.0f;
    if (pan > 1.0f) pan = 1.0f;
    Uint8 left = static_cast<Uint8>(255.0f * (pan > 0.0f ? 1.0f - pan : 1.0f));
    Uint8 right = static_cast<Uint8>(255.0f * (pan < 0.0f ? 1.0f + pan : 1.0f));
    Mix_SetPanning(s.channel, left, right);
}

static unsigned newSoundId()
{
    do {
        ++NextSoundId;
        if (NextSoundId == 0) ++NextSoundId;
    } while (Sounds.find(NextSoundId) != Sounds.end());
    return NextSoundId;
}

static void freeMusic(MusicState &m)
{
    cancelDecode(m.pending);
    cancelDecode(m.pendingLoop);
    if (m.channel >= 0)
        Mix_HaltChannel(m.channel);
    if (m.chunk)
        Mix_FreeChunk(m.chunk);
    m.chunk = nullptr;
    m.channel = -1;
}

/* A reserved music channel that no other theme holds. */
static int freeMusicChannel(const MusicState &self)
{
    for (int channel = 0; channel < MusicChannels; ++channel) {
        bool taken = Mix_Playing(channel) != 0;
        for (auto &p : Music)
            if (&p.second != &self && p.second.channel == channel)
                taken = true;
        if (!taken)
            return channel;
    }
    return -1;
}

/* Plays a decoded chunk on a music channel; takes ownership of chunk. */
static bool playMusicChunk(MusicState &m, Mix_Chunk *chunk, int loops)
{
    if (!chunk)
        return false;
    if (m.channel >= 0)
        Mix_HaltChannel(m.channel);
    int channel = freeMusicChannel(m);
    if (channel >= 0)
        channel = Mix_PlayChannel(channel, chunk, loops);
    if (channel < 0) {
        Mix_FreeChunk(chunk);
        return false;
    }
    if (m.chunk)
        Mix_FreeChunk(m.chunk);
    m.chunk = chunk;
    m.channel = channel;
    Mix_Volume(channel, mixVolume(m.localVolume * m.globalVolume));
    return true;
}

} // namespace

bool vw_InitAudio()
{
    if (AudioReady)
        return true;
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
        std::cerr << "vw_InitAudio(): SDL audio init failed: " << SDL_GetError() << "\n";
        return false;
    }
    const int wanted = MIX_INIT_OGG;
    const int got = Mix_Init(wanted);
    if ((got & wanted) != wanted) {
        std::cerr << "vw_InitAudio(): SDL2_mixer Ogg decoder unavailable: " << Mix_GetError() << "\n";
        Mix_Quit();
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        return false;
    }
    if (Mix_OpenAudio(44100, AUDIO_S16SYS, 2, 1024) != 0) {
        std::cerr << "vw_InitAudio(): Mix_OpenAudio failed: " << Mix_GetError() << "\n";
        Mix_Quit();
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        return false;
    }
    Mix_AllocateChannels(64);
    Mix_ReserveChannels(MusicChannels);
    DecodeLock = SDL_CreateMutex();
    DecodeWake = SDL_CreateCond();
    if (DecodeLock && DecodeWake)
        DecodeThread = SDL_CreateThread(decodeMain, "astromenace-music", nullptr);
    if (!DecodeThread) {
        std::cerr << "vw_InitAudio(): no music decode thread: " << SDL_GetError() << "\n";
        if (DecodeWake) SDL_DestroyCond(DecodeWake);
        if (DecodeLock) SDL_DestroyMutex(DecodeLock);
        DecodeWake = nullptr;
        DecodeLock = nullptr;
    }
    AudioReady = true;
    std::cout << "Audio      : AmigaChrome OpenGPU SDL2_mixer\n";
    return true;
}

bool vw_GetAudioStatus()
{
    return AudioReady;
}

void vw_ShutdownAudio()
{
    if (!AudioReady)
        return;
    vw_ReleaseAllSounds();
    vw_ReleaseAllMusic();
    stopDecodeThread();
    for (auto &p : SoundBuffers)
        if (p.second.chunk) Mix_FreeChunk(p.second.chunk);
    SoundBuffers.clear();
    Mix_CloseAudio();
    Mix_Quit();
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
    AudioReady = false;
}

void vw_Listener(float (&position)[3], float (&)[3], float (&)[6])
{
    ListenerX = position[0];
    ListenerY = position[1];
    ListenerZ = position[2];
    for (auto &p : Sounds)
        applySpatial(p.second);
}

unsigned int vw_LoadSoundBuffer(const std::string &name)
{
    BufferEntry *entry = bufferFor(name);
    return entry ? entry->id : 0;
}

void vw_ReleaseSoundBuffer(const std::string &name)
{
    for (auto it = Sounds.begin(); it != Sounds.end();) {
        if (it->second.name == name) {
            if (it->second.channel >= 0) Mix_HaltChannel(it->second.channel);
            it = Sounds.erase(it);
        } else {
            ++it;
        }
    }
    auto it = SoundBuffers.find(name);
    if (it != SoundBuffers.end()) {
        if (it->second.chunk) Mix_FreeChunk(it->second.chunk);
        SoundBuffers.erase(it);
    }
}

unsigned int vw_PlaySound(const std::string &name, float localVolume, float globalVolume,
                          const sVECTOR3D &location, bool relative, bool allowStop, int atType)
{
    BufferEntry *entry = bufferFor(name);
    if (!AudioReady || !entry || !entry->chunk)
        return 0;
    int channel = Mix_PlayChannel(-1, entry->chunk, 0);
    if (channel < 0)
        return 0;

    unsigned id = newSoundId();
    SoundState state{name, channel, localVolume, globalVolume, allowStop, relative, atType,
                     location.x, location.y, location.z, SDL_GetTicks(), 0, 0};
    Sounds.emplace(id, state);
    applySpatial(Sounds[id]);
    return id;
}

bool vw_IsSoundAvailable(unsigned int id)
{
    return id != 0 && Sounds.find(id) != Sounds.end();
}

unsigned int vw_ReplayFirstFoundSound(const std::string &name)
{
    for (auto &p : Sounds) {
        if (p.second.name == name) {
            BufferEntry *entry = bufferFor(name);
            if (!entry) return 0;
            int channel = Mix_PlayChannel(-1, entry->chunk, 0);
            if (channel < 0) return 0;
            p.second.channel = channel;
            p.second.lastTick = SDL_GetTicks();
            p.second.destroyPeriod = 0;
            p.second.destroyTicks = 0;
            applySpatial(p.second);
            return p.first;
        }
    }
    return 0;
}

void vw_SetSoundGlobalVolume(const std::string &name, float volume)
{
    for (auto &p : Sounds)
        if (p.second.name == name) {
            p.second.globalVolume = volume;
            applySpatial(p.second);
        }
}

void vw_SetSoundLocation(unsigned int id, float x, float y, float z)
{
    auto it = Sounds.find(id);
    if (it == Sounds.end()) return;
    it->second.x = x; it->second.y = y; it->second.z = z;
    applySpatial(it->second);
}

void vw_StopSound(unsigned int id, uint32_t delay)
{
    auto it = Sounds.find(id);
    if (it == Sounds.end()) return;
    if (delay == 0) {
        Mix_HaltChannel(it->second.channel);
    } else {
        it->second.destroyPeriod = delay;
        it->second.destroyTicks = 0;
    }
}

void vw_UpdateSound(uint32_t now)
{
    for (auto it = Sounds.begin(); it != Sounds.end();) {
        SoundState &s = it->second;
        uint32_t delta = now - s.lastTick;
        s.lastTick = now;
        bool erase = false;
        if (s.destroyPeriod) {
            s.destroyTicks += delta;
            if (s.destroyTicks >= s.destroyPeriod) {
                Mix_HaltChannel(s.channel);
                erase = true;
            } else {
                float remain = 1.0f - static_cast<float>(s.destroyTicks) /
                                       static_cast<float>(s.destroyPeriod);
                float old = s.localVolume;
                s.localVolume *= remain;
                applySpatial(s);
                s.localVolume = old;
            }
        }
        if (!erase && s.channel >= 0 && !Mix_Playing(s.channel))
            erase = true;
        if (erase) it = Sounds.erase(it); else ++it;
    }
}

void vw_ReleaseAllSounds()
{
    for (auto &p : Sounds)
        if (p.second.channel >= 0) Mix_HaltChannel(p.second.channel);
    Sounds.clear();
}

void vw_StopAllSoundsIfAllowed()
{
    for (auto it = Sounds.begin(); it != Sounds.end();) {
        if (it->second.allowedStop) {
            if (it->second.channel >= 0) Mix_HaltChannel(it->second.channel);
            it = Sounds.erase(it);
        } else {
            ++it;
        }
    }
}

bool vw_PlayMusic(const std::string &name, float localVolume, float globalVolume,
                  bool loop, const std::string &loopFileName)
{
    if (!AudioReady || name.empty())
        return false;
    if (Music.find(name) != Music.end())
        return true;

    MusicState m{nullptr, -1, nullptr, nullptr, 0, localVolume, globalVolume, loop, loopFileName,
                 false, false, localVolume, localVolume, 0, 0, SDL_GetTicks()};
    m.pending = queueDecode(name);
    if (!m.pending)
        return false;
    m.pendingLoops = loop ? -1 : 0;
    if (!loopFileName.empty())
        m.pendingLoop = queueDecode(loopFileName);
    Music.emplace(name, m);
    return true;
}

void vw_SetMusicGlobalVolume(float volume)
{
    for (auto &p : Music) {
        p.second.globalVolume = volume;
        if (p.second.channel >= 0)
            Mix_Volume(p.second.channel, mixVolume(p.second.localVolume * volume));
    }
}

/* A theme still being decoded (or waiting for its loop part) counts as
   playing: it has started, as an OpenAL stream has once queued. */
static bool musicActive(const MusicState &m)
{
    return m.pending || m.pendingLoop || (m.channel >= 0 && Mix_Playing(m.channel));
}

bool vw_IsMusicPlaying(const std::string &name)
{
    auto it = Music.find(name);
    return it != Music.end() && musicActive(it->second);
}

bool vw_IsAnyMusicPlaying()
{
    for (auto &p : Music)
        if (musicActive(p.second))
            return true;
    return false;
}

void vw_FadeOutAllMusicWithException(const std::string &name, uint32_t ticks,
                                     float exceptionEnd, uint32_t exceptionTicks)
{
    for (auto &p : Music) {
        MusicState &m = p.second;
        if (p.first == name) {
            if (m.fadeOut && exceptionEnd > 0.0f) {
                m.fadeOut = false;
                m.fadeIn = true;
                m.fadeStart = m.localVolume;
                m.fadeEnd = exceptionEnd;
                m.fadeTicks = 0;
                m.fadePeriod = exceptionTicks ? exceptionTicks : 1;
            }
        } else if (!m.fadeOut) {
            m.fadeIn = false;
            m.fadeOut = true;
            m.fadeStart = m.localVolume;
            m.fadeEnd = 0.0f;
            m.fadeTicks = 0;
            m.fadePeriod = ticks ? ticks : 1;
        }
    }
}

void vw_SetMusicFadeIn(const std::string &name, float endVolume, uint32_t ticks)
{
    auto it = Music.find(name);
    if (it == Music.end() || it->second.fadeIn) return;
    MusicState &m = it->second;
    m.fadeOut = false;
    m.fadeIn = true;
    m.fadeStart = m.localVolume;
    m.fadeEnd = endVolume;
    m.fadeTicks = 0;
    m.fadePeriod = ticks ? ticks : 1;
    m.lastTick = SDL_GetTicks();
}

void vw_UpdateMusic(uint32_t now)
{
    for (auto it = Music.begin(); it != Music.end();) {
        MusicState &m = it->second;
        uint32_t delta = now - m.lastTick;
        m.lastTick = now;
        bool erase = false;

        if (m.pending) {
            Mix_Chunk *chunk = nullptr;
            if (m.fadeOut) {
                erase = true;           // replaced before it was heard
            } else if (collectDecode(m.pending, chunk)) {
                m.pending.reset();
                if (!playMusicChunk(m, chunk, m.pendingLoops))
                    erase = true;
            }
            if (erase || m.pending) {
                if (erase) {
                    freeMusic(m);
                    it = Music.erase(it);
                } else {
                    ++it;               // fades start once it plays
                }
                continue;
            }
            delta = 0;
        }

        if (m.fadeIn || m.fadeOut) {
            m.fadeTicks += delta;
            float t = static_cast<float>(m.fadeTicks) / static_cast<float>(m.fadePeriod);
            if (t > 1.0f) t = 1.0f;
            m.localVolume = m.fadeStart + (m.fadeEnd - m.fadeStart) * t;
            if (m.channel >= 0)
                Mix_Volume(m.channel, mixVolume(m.localVolume * m.globalVolume));
            if (t >= 1.0f) {
                if (m.fadeOut) {
                    Mix_HaltChannel(m.channel);
                    erase = true;
                }
                m.fadeIn = false;
                m.fadeOut = false;
            }
        }

        if (!erase && m.channel >= 0 && !Mix_Playing(m.channel)) {
            if (!m.loopPart.empty() && m.pendingLoop) {
                Mix_Chunk *chunk = nullptr;
                if (collectDecode(m.pendingLoop, chunk)) {
                    m.pendingLoop.reset();
                    m.loopPart.clear();
                    m.looped = true;
                    if (!playMusicChunk(m, chunk, -1))
                        erase = true;
                }
                /* else the loop part is still decoding: try again next update */
            } else if (!m.loopPart.empty()) {
                erase = true;           // its file could not be read
            } else if (!m.looped) {
                erase = true;
            }
        }

        if (erase) {
            freeMusic(m);
            it = Music.erase(it);
        } else {
            ++it;
        }
    }
}

void vw_ReleaseMusic(const std::string &name)
{
    auto it = Music.find(name);
    if (it == Music.end()) return;
    freeMusic(it->second);
    Music.erase(it);
}

void vw_ReleaseAllMusic()
{
    for (auto &p : Music)
        freeMusic(p.second);
    Music.clear();
}

} // namespace viewizard
