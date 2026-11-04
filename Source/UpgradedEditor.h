#pragma once
#include "UpgradedProcessor.h"
class PhysicalModelGuitarAudioProcessorEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
  public:
    explicit PhysicalModelGuitarAudioProcessorEditor(PhysicalModelGuitarAudioProcessor &);
    ~PhysicalModelGuitarAudioProcessorEditor() override;
    void paint(juce::Graphics &) override;
    void resized() override;
    int getControlParameterIndex(juce::Component &) override;
    bool controlsFit() const;

  private:
    class Canvas;
    std::unique_ptr<Canvas> canvas;
    PhysicalModelGuitarAudioProcessor &processor;
    void timerCallback() override;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PhysicalModelGuitarAudioProcessorEditor)
};
