#include "audio/AudioManager.h"
#include "events/EventBus.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <memory>

int main() {
    std::cout << "[audio_test] Initializing extended audio test.\n";
    
    // We instantiate our newly redesigned miniaudio AudioManager
    auto eventBus = EventBus::create();
    AudioManager audio;
    
    if (!audio.init(eventBus)) {
        std::cerr << "[audio_test] Initialization failed.\n";
        return 1;
    }

    std::cout << "[audio_test] Starting ambience...\n";
    audio.startAmbience();

    std::cout << "[audio_test] Playing for 30 seconds. Listen carefully to see if it chops up!\n";
    for(int i = 1; i <= 30; i++) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        std::cout << "[audio_test] " << i << "s elapsed...\n";

        // Every 3 seconds, fire an extra debug noise to see if mixing works
        if (i % 3 == 0) {
            audio.playDebugProbe();
        }
    }

    std::cout << "[audio_test] Test complete. Shutting down.\n";
    audio.shutdown();
    return 0;
}
