#pragma once
#include <JuceHeader.h>

// Centralised colour palette + minimal custom drawing (chevron combo arrow,
// flat tooltip).  Everything else uses JUCE_V4 defaults configured via the
// setColour calls in the constructor — flat surfaces, no gradients.
class WillyBeatLookAndFeel : public juce::LookAndFeel_V4
{
public:
    WillyBeatLookAndFeel();

    void drawComboBox (juce::Graphics&, int width, int height,
                       bool isButtonDown,
                       int buttonX, int buttonY, int buttonW, int buttonH,
                       juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;

    // Skip the background fill and outline for a Slider's value-display
    // label so the readout number sits naked over the window.
    void drawLabel (juce::Graphics&, juce::Label&) override;

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos, float startAngle, float endAngle,
                           juce::Slider&) override;

    void drawButtonBackground (juce::Graphics&, juce::Button&,
                               const juce::Colour& backgroundColour,
                               bool isMouseOverButton, bool isButtonDown) override;

    void drawTooltip (juce::Graphics&, const juce::String& text,
                      int width, int height) override;

    juce::Font getTextButtonFont (juce::TextButton&, int) override;
    juce::Font getComboBoxFont   (juce::ComboBox&)         override;
    juce::Font getPopupMenuFont()                          override;

    // Tokyo-Night-inspired palette (matches WillyArp's feel).
    static const juce::Colour bgWindow;
    static const juce::Colour bgPanel;
    static const juce::Colour bgRecess;
    static const juce::Colour bgRaised;
    static const juce::Colour border;
    static const juce::Colour borderBright;
    static const juce::Colour accent;
    static const juce::Colour accentBright;
    static const juce::Colour accentSoft;
    static const juce::Colour textPrimary;
    static const juce::Colour textSecondary;
    static const juce::Colour textMuted;
};
