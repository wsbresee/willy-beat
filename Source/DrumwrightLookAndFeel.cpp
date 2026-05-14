#include "DrumwrightLookAndFeel.h"

// Tokyo-Night palette — flat, lavender accent, soft text.
const juce::Colour DrumwrightLookAndFeel::bgWindow      { 0xff1a1b26 };
const juce::Colour DrumwrightLookAndFeel::bgPanel       { 0xff24283b };
const juce::Colour DrumwrightLookAndFeel::bgRecess      { 0xff16161e };
const juce::Colour DrumwrightLookAndFeel::bgRaised      { 0xff2a2e42 };
const juce::Colour DrumwrightLookAndFeel::border        { 0xff414868 };
const juce::Colour DrumwrightLookAndFeel::borderBright  { 0xff565f89 };
const juce::Colour DrumwrightLookAndFeel::accent        { 0xffa78bfa };
const juce::Colour DrumwrightLookAndFeel::accentBright  { 0xffc4b5fd };
const juce::Colour DrumwrightLookAndFeel::accentSoft    { 0xff7c6bc8 };
const juce::Colour DrumwrightLookAndFeel::textPrimary   { 0xffc0caf5 };
const juce::Colour DrumwrightLookAndFeel::textSecondary { 0xff9aa5ce };
const juce::Colour DrumwrightLookAndFeel::textMuted     { 0xff6b7394 };

//==============================================================================

DrumwrightLookAndFeel::DrumwrightLookAndFeel()
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
    setColour (juce::ComboBox::backgroundColourId,        bgRaised);
    setColour (juce::ComboBox::textColourId,              textPrimary);
    setColour (juce::ComboBox::outlineColourId,           border);
    setColour (juce::ComboBox::arrowColourId,             accent);
    setColour (juce::ComboBox::buttonColourId,            juce::Colours::transparentBlack);

    // ── Buttons ──────────────────────────────────────────────────────────
    setColour (juce::TextButton::buttonColourId,          bgRaised);
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

void DrumwrightLookAndFeel::drawRotarySlider (juce::Graphics& g,
                                              int x, int y, int width, int height,
                                              float sliderPos,
                                              float startAngle, float endAngle,
                                              juce::Slider&)
{
    const float radius   = juce::jmin (width * 0.5f, height * 0.5f) * 0.82f;
    const float cx       = x + width  * 0.5f;
    const float cy       = y + height * 0.5f;

    // ── Body ─────────────────────────────────────────────────────────────────
    // Dark disc with a very subtle inner shadow ring.
    g.setColour (bgRecess);
    g.fillEllipse (cx - radius, cy - radius, radius * 2.0f, radius * 2.0f);

    // Thin rim — gives the disc a recessed-plate feel.
    g.setColour (border.withAlpha (0.6f));
    g.drawEllipse (cx - radius, cy - radius, radius * 2.0f, radius * 2.0f, 1.0f);

    // ── Track + active arc ────────────────────────────────────────────────────
    const float trackR   = radius * 0.78f;
    const float arcW     = radius * 0.14f;  // stroke width

    // Background track (full range, dim).
    juce::Path track;
    track.addCentredArc (cx, cy, trackR, trackR, 0.0f, startAngle, endAngle, true);
    g.setColour (borderBright.withAlpha (0.25f));
    g.strokePath (track, juce::PathStrokeType (arcW,
                  juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Active arc (value).
    const float valueAngle = startAngle + sliderPos * (endAngle - startAngle);
    if (sliderPos > 0.001f)
    {
        juce::Path active;
        active.addCentredArc (cx, cy, trackR, trackR, 0.0f, startAngle, valueAngle, true);
        g.setColour (accent);
        g.strokePath (active, juce::PathStrokeType (arcW,
                      juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // ── Indicator dot at value tip ────────────────────────────────────────────
    const float dotR  = arcW * 0.72f;
    const float dotPx = cx + trackR * std::sin (valueAngle);
    const float dotPy = cy - trackR * std::cos (valueAngle);
    g.setColour (sliderPos > 0.001f ? accentBright : borderBright.withAlpha (0.5f));
    g.fillEllipse (dotPx - dotR, dotPy - dotR, dotR * 2.0f, dotR * 2.0f);
}

void DrumwrightLookAndFeel::drawLabel (juce::Graphics& g, juce::Label& label)
{
    // Slider value labels: draw the text only - no background fill, no
    // outline rectangle, even when the underlying LookAndFeel_V4 default
    // would draw one. Other labels (rotary captions etc.) fall through to
    // the inherited path.
    if (dynamic_cast<juce::Slider*> (label.getParentComponent()) == nullptr)
    {
        juce::LookAndFeel_V4::drawLabel (g, label);
        return;
    }

    if (label.isBeingEdited()) return; // a TextEditor takes over while editing

    auto font = getLabelFont (label);
    g.setColour (label.findColour (juce::Label::textColourId)
                     .withMultipliedAlpha (label.isEnabled() ? 1.0f : 0.5f));
    g.setFont (font);
    auto textArea = getLabelBorderSize (label).subtractedFrom (label.getLocalBounds());
    g.drawFittedText (label.getText(), textArea, label.getJustificationType(),
                      juce::jmax (1, (int) ((float) textArea.getHeight() / font.getHeight())),
                      label.getMinimumHorizontalScale());
}

void DrumwrightLookAndFeel::fillTextEditorBackground (juce::Graphics& g,
                                                      int width, int height,
                                                      juce::TextEditor& te)
{
    auto b = juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height).reduced (0.5f);
    g.setColour (te.findColour (juce::TextEditor::backgroundColourId));
    g.fillRoundedRectangle (b, 6.0f);
}

void DrumwrightLookAndFeel::drawTextEditorOutline (juce::Graphics& g,
                                                   int width, int height,
                                                   juce::TextEditor& te)
{
    if (! te.isEnabled()) return;
    auto b = juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height).reduced (0.5f);
    g.setColour (te.hasKeyboardFocus (true)
                     ? te.findColour (juce::TextEditor::focusedOutlineColourId)
                     : te.findColour (juce::TextEditor::outlineColourId));
    g.drawRoundedRectangle (b, 6.0f, 1.0f);
}

void DrumwrightLookAndFeel::drawTooltip (juce::Graphics& g, const juce::String& text,
                                         int width, int height)
{
    auto bounds = juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height).reduced (0.5f);

    g.setColour (bgPanel.brighter (0.04f));
    g.fillRoundedRectangle (bounds, 6.0f);

    g.setColour (borderBright.withAlpha (0.50f));
    g.drawRoundedRectangle (bounds, 6.0f, 1.0f);

    g.setColour (textSecondary);
    g.setFont (juce::Font (juce::FontOptions{}.withHeight (12.0f)));
    g.drawFittedText (text,
                      juce::Rectangle<int> (8, 2, width - 16, height - 4),
                      juce::Justification::centredLeft, 3);
}

void DrumwrightLookAndFeel::drawButtonBackground (juce::Graphics& g,
                                                  juce::Button& button,
                                                  const juce::Colour& /*backgroundColour*/,
                                                  bool isMouseOverButton,
                                                  bool isButtonDown)
{
    auto bounds = button.getLocalBounds().toFloat().reduced (0.5f);
    const bool on       = button.getToggleState();
    const float enableA = button.isEnabled() ? 1.0f : 0.45f;

    juce::Colour fill = (on
        ? button.findColour (juce::TextButton::buttonOnColourId)
        : button.findColour (juce::TextButton::buttonColourId)).withMultipliedAlpha (enableA);

    if (isButtonDown)
        fill = fill.darker (0.18f);
    else if (isMouseOverButton)
        fill = fill.brighter (0.12f);

    g.setColour (fill);
    g.fillRoundedRectangle (bounds, 6.0f);

    // Accent-filled buttons get a bright rim; dark-filled buttons get a subtle border.
    g.setColour (fill.getPerceivedBrightness() > 0.35f
                     ? accentBright.withAlpha (0.40f)
                     : borderBright.withAlpha (0.25f));
    g.drawRoundedRectangle (bounds, 6.0f, 1.0f);
}

void DrumwrightLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height,
                                         bool /*isButtonDown*/,
                                         int /*bx*/, int /*by*/, int /*bw*/, int /*bh*/,
                                         juce::ComboBox& box)
{
    auto bounds = juce::Rectangle<float> (0.0f, 0.0f,
                                           (float) width, (float) height).reduced (0.5f);

    g.setColour (box.findColour (juce::ComboBox::backgroundColourId));
    g.fillRoundedRectangle (bounds, 6.0f);

    g.setColour (box.findColour (juce::ComboBox::outlineColourId));
    g.drawRoundedRectangle (bounds, 6.0f, 1.0f);

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

void DrumwrightLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (juce::Rectangle<int> (8, 0, box.getWidth() - 22, box.getHeight()));
    label.setFont (getComboBoxFont (box));
}

//==============================================================================

juce::Font DrumwrightLookAndFeel::getTextButtonFont (juce::TextButton&, int)
{
    return juce::Font (juce::FontOptions{}.withHeight (12.0f));
}

juce::Font DrumwrightLookAndFeel::getComboBoxFont (juce::ComboBox&)
{
    return juce::Font (juce::FontOptions{}.withHeight (12.0f));
}

juce::Font DrumwrightLookAndFeel::getPopupMenuFont()
{
    return juce::Font (juce::FontOptions{}.withHeight (12.5f));
}
