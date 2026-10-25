#ifndef IRON_CIRCUIT_AUDIO_AUDIOMANAGER_H
#define IRON_CIRCUIT_AUDIO_AUDIOMANAGER_H

#include "events/IEventListener.h"
#include <memory>
#include <string>
#include "audio/miniaudio.h"

class EventBus;

class AudioManager : public IEventListener {
public:
    AudioManager();
    ~AudioManager() override;

    bool init(std::shared_ptr<EventBus> eventBus);
    void startAmbience();
    void stopAmbience();
    void playPlayerAttack(bool ranged);
    void playEnemyAttack(bool ranged);
    void playDebugProbe();
    void shutdown();
    void onEvent(const GameEvent& event) override;

private:
    enum class Cue {
        PlayerMelee,
        PlayerRanged,
        EnemyMelee,
        EnemyRanged,
        RobotDestroyed,
        PlayerDefeated,
    };

    void unsubscribe();
    void playCue(Cue cue);
    void loadSoundInto(ma_sound* sound, const std::string& path, float volume, bool loop);
    ma_sound* soundForCue(Cue cue);

    std::shared_ptr<EventBus> m_eventBus;
    ma_engine m_engine;
    bool m_engineInitialized = false;

    ma_sound m_playerMelee;
    ma_sound m_playerRanged;
    ma_sound m_enemyMelee;
    ma_sound m_enemyRanged;
    ma_sound m_robotDestroyed;
    ma_sound m_playerDefeated;
    ma_sound m_ambience;

    bool m_enabled = false;
};

#endif // IRON_CIRCUIT_AUDIO_AUDIOMANAGER_H
