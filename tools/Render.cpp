#include "UpgradedProcessor.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>
namespace fs = std::filesystem;
void set(PhysicalModelGuitarAudioProcessor &p, const char *id, float v)
{
    auto *par = p.parameters.getParameter(id);
    par->setValueNotifyingHost(par->convertTo0to1(v));
}
int main(int argc, char **argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    if (argc != 2)
        return 2;
    fs::create_directories(argv[1]);
    std::ofstream stats(fs::path(argv[1]) / "native_metrics.csv");
    stats << "model,demo,rate,rms,peak\n";
    for (int model = 0; model < 4; ++model)
        for (int demo = 0; demo < 4; ++demo)
        {
            auto p = std::make_unique<PhysicalModelGuitarAudioProcessor>();
            set(*p, "model", float(model));
            p->setRateAndBufferSizeDetails(48000, 257);
            p->prepareToPlay(48000, 257);
            if (demo == 3)
            {
                p->loadComparisonPreset(model);
                p->prepareToPlay(48000, 257);
            }
            struct Event
            {
                int sample, note, velocity;
            };
            std::vector<Event> events;
            if (demo == 0 || demo == 3)
            {
                int notes[] = {40, 47, 52, 55, 59, 64};
                for (int i = 0; i < 6; ++i)
                {
                    events.push_back({i * 24000, notes[i], 100});
                    events.push_back({i * 24000 + 20000, notes[i], 0});
                }
            }
            if (demo == 1)
                for (int note : {40, 47, 52, 56, 59, 64})
                {
                    events.push_back({0, note, 100});
                    events.push_back({96000, note, 0});
                }
            if (demo == 2)
            {
                events.push_back({0, 52, 100});
                events.push_back({144000, 52, 0});
            }
            const int frames = 48000 * 8;
            juce::AudioBuffer<float> audio(2, frames);
            audio.clear();
            for (int pos = 0; pos < frames; pos += 257)
            {
                const int n = std::min(257, frames - pos);
                juce::AudioBuffer<float> b(2, n);
                juce::MidiBuffer midi;
                for (auto e : events)
                    if (e.sample >= pos && e.sample < pos + n)
                        midi.addEvent(e.velocity
                                          ? juce::MidiMessage::noteOn(1, e.note, juce::uint8(e.velocity))
                                          : juce::MidiMessage::noteOff(1, e.note),
                                      e.sample - pos);
                p->processBlock(b, midi);
                for (int ch = 0; ch < 2; ++ch)
                    audio.copyFrom(ch, pos, b, ch, 0, n);
            }
            const char *names[] = {"Melody", "Chord", "HeldE3", "Comparison"};
            const auto name = "Model" + std::to_string(model) + "_" + names[demo];
            juce::File file(juce::String((fs::path(argv[1]) / (name + ".wav")).string()));
            auto fileStream = file.createOutputStream();
            if (!fileStream)
                return 3;
            if (!fileStream->setPosition(0) || fileStream->truncate().failed())
                return 3;
            std::unique_ptr<juce::OutputStream> stream = std::move(fileStream);
            juce::WavAudioFormat wav;
            auto writer = wav.createWriterFor(
                stream, juce::AudioFormatWriterOptions()
                            .withSampleRate(48000)
                            .withNumChannels(2)
                            .withBitsPerSample(32)
                            .withSampleFormat(juce::AudioFormatWriterOptions::SampleFormat::floatingPoint));
            if (!writer || !writer->writeFromAudioSampleBuffer(audio, 0, frames))
                return 4;
            const float rms = audio.getRMSLevel(0, 0, frames), peak = audio.getMagnitude(0, frames);
            stats << model << "," << names[demo] << ",48000," << rms << "," << peak << "\n";
            std::cout << name << " rms=" << rms << " peak=" << peak << "\n";
            if (demo == 0)
            {
                juce::MemoryBlock state;
                p->getStateInformation(state);
                std::ofstream f(fs::path(argv[1]) / ("Model" + std::to_string(model) + "_Defaults.state"),
                                std::ios::binary);
                f.write(static_cast<const char *>(state.getData()), std::streamsize(state.getSize()));
            }
            if (demo == 3)
            {
                juce::MemoryBlock state;
                p->getStateInformation(state);
                std::ofstream f(fs::path(argv[1]) / ("Model" + std::to_string(model) + "_Comparison.state"),
                                std::ios::binary);
                f.write(static_cast<const char *>(state.getData()), std::streamsize(state.getSize()));
            }
        }
}
