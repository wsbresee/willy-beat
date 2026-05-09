#include "WillyBeatLookAndFeel.h"

// Tokyo-Night palette — flat, lavender accent, soft text.
const juce::Colour WillyBeatLookAndFeel::bgWindow      { 0xff1a1b26 };
const juce::Colour WillyBeatLookAndFeel::bgPanel       { 0xff24283b };
const juce::Colour WillyBeatLookAndFeel::bgRecess      { 0xff16161e };
const juce::Colour WillyBeatLookAndFeel::bgRaised      { 0xff2a2e42 };
const juce::Colour WillyBeatLookAndFeel::border        { 0xff414868 };
const juce::Colour WillyBeatLookAndFeel::borderBright  { 0xff565f89 };
const juce::Colour WillyBeatLookAndFeel::accent        { 0xffa78bfa };
const juce::Colour WillyBeatLookAndFeel::accentBright  { 0xffc4b5fd };
const juce::Colour WillyBeatLookAndFeel::accentSoft    { 0xff7c6bc8 };
const juce::Colour WillyBeatLookAndFeel::textPrimary   { 0xffc0caf5 };
const juce::Colour WillyBeatLookAndFeel::textSecondary { 0xff9aa5ce };
const juce::Colour WillyBeatLookAndFeel::textMuted     { 0xff6b7394 };

//==============================================================================

WillyBeatLookAndFeel::WillyBeatLookAndFeel()
{
    // ── Slider / rotary (uses JUCE V4 default drawing) ───────────────────
    setColour (juce::Slider::rotarySliderFillColourId,    accent);
    setColour (juce::Slider::rotarySliderOutlineColourId, bgPanel);
    setColour (juce::Slider::thumbColourId,               accent);
    setColour (juce::Slider::backgroundColourId,          bgPanel);
    setColour (juce::Slider::trackColourId,               accent);
    setColour (juce::Slider::textBoxTextColourId,         textMuted);
    setColour (juce::Slider::textBoxOutlineColourId,      juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId,   juce::Colours::transparentBlack);

    // ── Combo box ────────────────────────────────────────────────────────
    setColour (juce::ComboBox::backgroundColourId,        bgPanel);
    setColour (juce::ComboBox::textColourId,              textPrimary);
    setColour (juce::ComboBox::outlineColourId,           border);
    setColour (juce::ComboBox::arrowColourId,             accent);
    setColour (juce::ComboBox::buttonColourId,            juce::Colours::transparentBlack);

    // ── Buttons ──────────────────────────────────────────────────────────
    setColour (juce::TextButton::buttonColourId,          bgPanel);
    setColour (juce::TextButton::buttonOnColourId,        accent);
    setColour (juce::TextButton::textColourOffId,         textPrimary);
    setColour (juce::TextButton::textColourOnId,          juce::Colours::white);

    // ── Text editors ─────────────────────────────────────────────────────
    setColour (juce::TextEditor::backgroundColourId,      bgRecess);
    setColour (juce::TextEditor::outlineColourId,         border);
    setColour (juce::TextEditor::focusedOutlineColourId,  accent);
    setColour (juce::TextEditor::textColourId,            textPrimary);
    setColour (juce::TextEditor::highlightColourId,       accent.withAlpha (0.30f));

    setColour (juce::Label::textColourId,                 textSecondary);

    // ── Popup menus ──────────────────────────────────────────────────────
    setColour (juce::PopupMenu::backgroundColourId,       bgPanel);
    setColour (juce::PopupMenu::textColourId,             textPrimary);
    setColour (juce::PopupMenu::headerTextColourId,       textSecondary);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, accent);
    setColour (juce::PopupMenu::highlightedTextColourId,  juce::Colours::white);

    // ── Tooltip ──────────────────────────────────────────────────────────
    setColour (juce::TooltipWindow::backgroundColourId,   bgPanel);
    setColour (juce::TooltipWindow::textColourId,         textPrimary);
    setColour (juce::TooltipWindow::outlineColourId,      borderBright);
}

//==============================================================================

void WillyBeatLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height,
                                         bool /*isButtonDown*/,
                                         int /*bx*/, int /*by*/, int /*bw*/, int /*bh*/,
                                         juce::ComboBox& box)
{
    auto bounds = juce::Rectangle<float> (0.0f, 0.0f,
                                           (float) width, (float) height).reduced (0.5f);

    g.setColour (box.findColour (juce::ComboBox::backgroundColourId));
    g.fillRoundedRectangle (bounds, 4.0f);

    g.setColour (box.findColour (juce::ComboBox::outlineColourId));
    g.drawRoundedRectangle (bounds, 4.0f, 1.0f);

    auto arrowZone = juce::Rectangle<float> ((float) width - 18.0f, 0.0f,
                                              14.0f, (float) height);
    const auto cx = arrowZone.getCentreX();
    const auto cy = arrowZone.getCentreY();

    juce::Path arrow;
    arrow.startNewSubPath (cx - 4.0f, cy - 2.0f);
    arrow.lineTo (cx,         cy + 2.5f);
    arrow.lineTo (cx + 4.0f,  cy - 2.0f);
    g.setColour (box.findColour (juce::ComboBox::arrowColourId));
    g.strokePath (arrow, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved,
                                                juce::PathStrokeType::rounded));
}

void WillyBeatLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (juce::Rectangle<int> (8, 0, box.getWidth() - 22, box.getHeight()));
    label.setFont (getComboBoxFont (box));
}

//==============================================================================

juce::Font WillyBeatLookAndFeel::getTextButtonFont (juce::TextButton&, int)
{
    return juce::Font (juce::FontOptions{}.withHeight (12.0f));
}

juce::Font WillyBeatLookAndFeel::getComboBoxFont (juce::ComboBox&)
{
    return juce::Font (juce::FontOptions{}.withHeight (12.0f));
}

juce::Font WillyBeatLookAndFeel::getPopupMenuFont()
{
    return juce::Font (juce::FontOptions{}.withHeight (12.5f));
}
