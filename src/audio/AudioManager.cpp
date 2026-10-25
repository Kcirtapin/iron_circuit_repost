#define MINIAUDIO_IMPLEMENTATION
#include "audio/miniaudio.h"
#include "audio/AudioManager.h"

#include "events/EventBus.h"
#include "model/EnemyRobot.h"
#include "model/PlayerRobot.h"
#include <filesystem>
#include <iostream>
#include <SDL.h>

namespace {
    std::string resolveAssetPath(const std::string& relativePath) {
        std::vector<std::filesystem::path> candidates;
        if (char* basePath = SDL_GetBasePath()) {
            const std::filesystem::path base(basePath);
            candidates.push_back(base / relativePath);
            candidates.push_back(base / ".." / relativePath);
            SDL_free(basePath);
        }
        candidates.push_back(std::filesystem::path(relativePath));
        candidates.push_back(std::filesystem::path("..") / relativePath);

        for (const auto& candidate : candidates) {
            if (std::filesystem::exists(candidate)) {
                return candidate.string();
            }
        }
        return {};
    }
}

AudioManager::AudioManager() = default;

AudioManager::~AudioManager() {
    shutdown();
}

bool AudioManager::init(std::shared_ptr<EventBus> eventBus) {
    shutdown();

    if (!eventBus) {
        std::cerr << "[Audio] No event bus available; audio disabled.\n";
        return false;
    }

    // Optional: We can customize the engine's config here if WSL specific problems pop up,
    // but typically miniaudio bypasses SDL's weird pulse buffering flaws out-of-the-box.
    ma_engine_config engineConfig = ma_engine_config_init();
    ma_result result = ma_engine_init(&engineConfig, &m_engine);
    if (result != MA_SUCCESS) {
        std::cerr << "[Audio] Failed to initialize miniaudio engine. Fallback audio disabled.\n";
        return false;
    }
    m_engineInitialized = true;

    // Load natively decoded chunks into RAM to prevent streaming choppiness on WSL
    loadSoundInto(&m_playerMelee, "resource/sound/daviddumaisaudio-sword-slash-and-swing-185432.mp3", 0.6f, false);
    loadSoundInto(&m_playerRanged, "resource/sound/kalsstockmedia-laser-gun-shot-283838.mp3", 0.5f, false);
    loadSoundInto(&m_enemyMelee, "resource/sound/floraphonic-metal-hit-10-193281.mp3", 0.6f, false);
    loadSoundInto(&m_enemyRanged, "resource/sound/ahmed_abdulaal-laser-312360.mp3", 0.5f, false);
    loadSoundInto(&m_robotDestroyed, "resource/sound/freesound_community-damage-40114.mp3", 0.7f, false);
    loadSoundInto(&m_playerDefeated, "resource/sound/floraphonic-metal-hit-10-193281.mp3", 0.8f, false);
    
    // Ambient looping sound logic - decoding entirely into memory so the WSL disk stream does not stutter!
    loadSoundInto(&m_ambience, "resource/sound/freesound_community-sci-fi-survival-dreamscape-6319.mp3", 0.9f, true);

    m_eventBus = std::move(eventBus);
    m_eventBus->subscribe(util::EventType::ROBOT_DEFEATED, this);
    m_enabled = true;
    std::cout << "[Audio] Ready (miniaudio max stability). Press F8 in-game to replay a test cue.\n";
    return true;
}

void AudioManager::shutdown() {
    unsubscribe();

    if (m_engineInitialized) {
        ma_sound_uninit(&m_playerMelee);
        ma_sound_uninit(&m_playerRanged);
        ma_sound_uninit(&m_enemyMelee);
        ma_sound_uninit(&m_enemyRanged);
        ma_sound_uninit(&m_robotDestroyed);
        ma_sound_uninit(&m_playerDefeated);
        ma_sound_uninit(&m_ambience);

        ma_engine_uninit(&m_engine);
        m_engineInitialized = false;
    }

    m_enabled = false;
    m_eventBus.reset();
}

void AudioManager::onEvent(const GameEvent& event) {
    if (!m_enabled) return;

    if (event.type == util::EventType::ROBOT_DEFEATED) {
        if (dynamic_cast<PlayerRobot*>(event.target)) {
            playCue(Cue::PlayerDefeated);
        } else {
            playCue(Cue::RobotDestroyed);
        }
    }
}

void AudioManager::startAmbience() {
    if (!m_enabled || !m_engineInitialized) return;
    if (!ma_sound_is_playing(&m_ambience)) {
        ma_sound_start(&m_ambience);
        std::cout << "[Audio] Ambience started playing via miniaudio.\n";
    }
}

void AudioManager::stopAmbience() {
    if (m_enabled && m_engineInitialized) {
        ma_sound_stop(&m_ambience);
    }
}

void AudioManager::playPlayerAttack(bool ranged) {
    if (!m_enabled) return;
    playCue(ranged ? Cue::PlayerRanged : Cue::PlayerMelee);
}

void AudioManager::playEnemyAttack(bool ranged) {
    if (!m_enabled) return;
    playCue(ranged ? Cue::EnemyRanged : Cue::EnemyMelee);
}

void AudioManager::playDebugProbe() {
    if (!m_enabled) return;
    playCue(Cue::PlayerRanged);
}

void AudioManager::unsubscribe() {
    if (!m_eventBus) return;
    m_eventBus->unsubscribe(util::EventType::ROBOT_DEFEATED, this);
}

void AudioManager::playCue(Cue cue) {
    if (ma_sound* sound = soundForCue(cue)) {
        // Restart the sound completely cleanly from the beginning
        if (ma_sound_is_playing(sound)) {
            ma_sound_stop(sound);
        }
        ma_sound_seek_to_pcm_frame(sound, 0);
        ma_sound_start(sound);
    }
}

void AudioManager::loadSoundInto(ma_sound* sound, const std::string& path, float volume, bool loop) {
    const std::string resolvedPath = resolveAssetPath(path);
    if (resolvedPath.empty()) {
        std::cerr << "[Audio] Could not resolve asset path for '" << path << "'\n";
        return;
    }

    // Decode directly into memory to prevent streaming choppiness from disk latency crossing OS boundary!
    ma_result result = ma_sound_init_from_file(&m_engine, resolvedPath.c_str(), MA_SOUND_FLAG_DECODE, NULL, NULL, sound);
    if (result == MA_SUCCESS) {
        ma_sound_set_volume(sound, volume);
        if (loop) {
            ma_sound_set_looping(sound, MA_TRUE);
        }
    } else {
        std::cerr << "[Audio] Failed to load '" << resolvedPath << "'\n";
    }
}

ma_sound* AudioManager::soundForCue(Cue cue) {
    switch (cue) {
        case Cue::PlayerMelee: return &m_playerMelee;
        case Cue::PlayerRanged: return &m_playerRanged;
        case Cue::EnemyMelee: return &m_enemyMelee;
        case Cue::EnemyRanged: return &m_enemyRanged;
        case Cue::RobotDestroyed: return &m_robotDestroyed;
        case Cue::PlayerDefeated: return &m_playerDefeated;
    }
    return nullptr;
}
