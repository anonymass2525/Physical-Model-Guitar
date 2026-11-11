#include "UpgradedEditor.h"
#include <cmath>
namespace
{
const juce::Colour ink(0xffe8e6de), muted(0xff95a3a6), panel(0xff19262c), background(0xff0d171d);
juce::Colour accent(int model)
{
    const juce::uint32 colours[] = {0xff66d4c0, 0xff9ba7ff, 0xffdfac69, 0xffd9a1a0};
    return juce::Colour(colours[juce::jlimit(0, 3, model)]);
}
void text(juce::Graphics &g, const juce::String &s, juce::Rectangle<int> b, float size, juce::Colour c,
          juce::Justification align = juce::Justification::centredLeft)
{
    g.setColour(c);
    g.setFont(juce::Font(juce::FontOptions(size)));
    g.drawFittedText(s, b, align, 1);
}
class Action final : public juce::Component
{
  public:
    Action(juce::String label, std::function<void()> fn) : caption(std::move(label)), action(std::move(fn))
    {
        setTitle(caption);
        setWantsKeyboardFocus(true);
    }
    void paint(juce::Graphics &g) override
    {
        auto b = getLocalBounds().toFloat().reduced(1);
        g.setColour(selected ? colour.withAlpha(.20f) : panel);
        g.fillRoundedRectangle(b, 7);
        g.setColour(selected ? colour : muted.withAlpha(.30f));
        g.drawRoundedRectangle(b, 7, hasKeyboardFocus(true) ? 2.f : 1.f);
        text(g, caption, getLocalBounds().reduced(8), 13, selected ? ink : muted,
             juce::Justification::centred);
    }
    void mouseDown(const juce::MouseEvent &) override
    {
        grabKeyboardFocus();
        if (action)
            action();
    }
    bool keyPressed(const juce::KeyPress &k) override
    {
        if (k == juce::KeyPress::returnKey || k == juce::KeyPress::spaceKey)
        {
            if (action)
                action();
            return true;
        }
        return false;
    }
    std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override
    {
        return std::make_unique<juce::AccessibilityHandler>(
            *this, juce::AccessibilityRole::button,
            juce::AccessibilityActions().addAction(juce::AccessibilityActionType::press,
                                                   [this]
                                                   {
                                                       if (action)
                                                           action();
                                                   }));
    }
    void setCaption(const juce::String& name){ caption=name; repaint(); }
    bool selected = false;
    juce::Colour colour = accent(0);

  private:
    juce::String caption;
    std::function<void()> action;
};
class Dial final : public juce::Component
{
  public:
    Dial(PhysicalModelGuitarAudioProcessor &p, const char *id, const char *name, const char *hint)
        : param(*p.parameters.getParameter(id)), caption(name), help(hint), attachment(
                                                                                param,
                                                                                [this](float v)
                                                                                {
                                                                                    value = v;
                                                                                    repaint();
                                                                                },
                                                                                &p.undo)
    {
        setTitle(param.getName(64));
        setDescription(hint);
        setWantsKeyboardFocus(true);
        attachment.sendInitialUpdate();
    }
    void paint(juce::Graphics &g) override
    {
        const float alpha = isEnabled() ? 1.f : .28f;
        const auto col = colour.withAlpha(alpha);
        const float n = param.convertTo0to1(value);
        const auto b = getLocalBounds();
        text(g, caption, b.withHeight(22), 11, muted.withAlpha(alpha), juce::Justification::centred);
        const float cx = getWidth() * .5f, cy = 57.f, r = 24.f;
        juce::Path path;
        path.addCentredArc(cx, cy, r, r, 0, 3.8f, 8.77f, true);
        g.setColour(muted.withAlpha(.16f * alpha));
        g.strokePath(path, juce::PathStrokeType(4));
        path.clear();
        path.addCentredArc(cx, cy, r, r, 0, 3.8f, 3.8f + n * 4.97f, true);
        g.setColour(col);
        g.strokePath(path, juce::PathStrokeType(4));
        const float a = 3.8f + n * 4.97f;
        g.drawLine(cx + std::sin(a) * 10, cy - std::cos(a) * 10, cx + std::sin(a) * 20, cy - std::cos(a) * 20,
                   2);
        text(g,
             param.getText(param.convertTo0to1(value), 16) +
                 (param.getLabel().isEmpty() ? "" : " " + param.getLabel()),
             {0, 84, b.getWidth(), 21}, 15, ink.withAlpha(alpha), juce::Justification::centred);
        text(g, isEnabled() ? help : "Not used by this model", {0, 108, b.getWidth(), 16}, 9,
             muted.withAlpha(alpha), juce::Justification::centred);
        if (hasKeyboardFocus(true))
        {
            g.setColour(col);
            g.drawRoundedRectangle(b.toFloat().reduced(1), 6, 1);
        }
    }
    void mouseDown(const juce::MouseEvent &) override
    {
        grabKeyboardFocus();
        dragStart = param.convertTo0to1(value);
        attachment.beginGesture();
    }
    void mouseDrag(const juce::MouseEvent &e) override
    {
        const float scale = e.mods.isShiftDown() ? .0007f : .004f;
        attachment.setValueAsPartOfGesture(param.convertFrom0to1(
            juce::jlimit(0.f, 1.f, dragStart - float(e.getDistanceFromDragStartY()) * scale)));
    }
    void mouseUp(const juce::MouseEvent &) override
    {
        attachment.endGesture();
    }
    void mouseDoubleClick(const juce::MouseEvent &) override
    {
        attachment.setValueAsCompleteGesture(param.convertFrom0to1(param.getDefaultValue()));
    }
    void mouseWheelMove(const juce::MouseEvent &, const juce::MouseWheelDetails &w) override
    {
        step(w.deltaY > 0 ? .01f : -.01f);
    }
    bool keyPressed(const juce::KeyPress &k) override
    {
        const int key = k.getKeyCode();
        const float v = k.getModifiers().isShiftDown() ? .001f : .01f;
        if (key == juce::KeyPress::upKey || key == juce::KeyPress::rightKey)
        {
            step(v);
            return true;
        }
        if (key == juce::KeyPress::downKey || key == juce::KeyPress::leftKey)
        {
            step(-v);
            return true;
        }
        if (key == juce::KeyPress::homeKey)
        {
            attachment.setValueAsCompleteGesture(param.convertFrom0to1(param.getDefaultValue()));
            return true;
        }
        return false;
    }
    struct AccessibleValue final : juce::AccessibilityValueInterface
    {
        explicit AccessibleValue(Dial &d) : dial(d) {}
        Dial &dial;
        bool isReadOnly() const override
        {
            return !dial.isEnabled();
        }
        double getCurrentValue() const override
        {
            return dial.value;
        }
        juce::String getCurrentValueAsString() const override
        {
            return dial.param.getText(dial.param.convertTo0to1(dial.value), 32);
        }
        void setValue(double v) override
        {
            if (dial.isEnabled())
                dial.attachment.setValueAsCompleteGesture(float(v));
        }
        void setValueAsString(const juce::String &s) override
        {
            setValue(dial.param.convertFrom0to1(dial.param.getValueForText(s)));
        }
        AccessibleValueRange getRange() const override
        {
            auto r = dial.param.getNormalisableRange();
            return {{r.start, r.end}, r.interval};
        }
    };
    std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override
    {
        return std::make_unique<juce::AccessibilityHandler>(
            *this, juce::AccessibilityRole::slider, juce::AccessibilityActions{},
            juce::AccessibilityHandler::Interfaces(std::make_unique<AccessibleValue>(*this)));
    }
    juce::RangedAudioParameter &param;
    juce::Colour colour = accent(0);

  private:
    void step(float delta)
    {
        attachment.setValueAsCompleteGesture(
            param.convertFrom0to1(juce::jlimit(0.f, 1.f, param.convertTo0to1(value) + delta)));
    }
    juce::String caption, help;
    float value = 0, dragStart = 0;
    juce::ParameterAttachment attachment;
};
void guitar(juce::Graphics &g, int model, juce::Rectangle<float> area, float peak)
{
    juce::Graphics::ScopedSaveState state(g);
    g.addTransform(juce::AffineTransform::scale(area.getWidth() / 680.f, area.getHeight() / 220.f)
                       .translated(area.getX(), area.getY()));
    juce::Path body;
    if (model == 1)
    {
        body.startNewSubPath(290, 88);
        body.cubicTo(250, 105, 260, 26, 230, 28);
        body.cubicTo(205, 30, 220, 66, 190, 64);
        body.cubicTo(160, 62, 138, 30, 105, 46);
        body.cubicTo(37, 77, 57, 177, 112, 192);
        body.cubicTo(155, 204, 187, 157, 215, 162);
        body.cubicTo(245, 166, 254, 211, 276, 184);
        body.cubicTo(303, 153, 259, 144, 291, 132);
    }
    else if (model == 3)
    {
        body.startNewSubPath(292, 89);
        body.cubicTo(256, 51, 224, 48, 205, 86);
        body.cubicTo(187, 112, 174, 54, 122, 49);
        body.cubicTo(35, 41, 34, 188, 121, 197);
        body.cubicTo(168, 202, 187, 142, 206, 158);
        body.cubicTo(249, 196, 289, 164, 292, 133);
    }
    else
    {
        body.startNewSubPath(290, 86);
        body.cubicTo(257, 44, 218, 49, 203, 81);
        body.cubicTo(189, 101, 172, 58, 131, 57);
        body.cubicTo(50, 53, 47, 176, 120, 185);
        body.cubicTo(160, 192, 186, 142, 205, 155);
        body.cubicTo(241, 179, 284, 169, 290, 132);
    }
    body.closeSubPath();
    g.setGradientFill(juce::ColourGradient(accent(model).darker(.65f), 115, 60,
                                           model == 0 ? panel : juce::Colour(0xff402c22), 260, 180, false));
    g.fillPath(body);
    g.setColour(accent(model));
    g.strokePath(body, juce::PathStrokeType(model == 0 ? 2.f : 1.2f));
    if (model != 1)
    {
        g.setColour(juce::Colour(0xff080e12));
        g.fillEllipse(190, 88, 49, 49);
        g.setColour(accent(model).withAlpha(.8f));
        g.drawEllipse(186, 84, 57, 57, model == 3 ? 3.f : 1.f);
    }
    g.setColour(juce::Colour(0xff4b3930));
    g.fillRoundedRectangle(253, 90, 354, 40, 4);
    g.setColour(juce::Colour(0xffd2baa0));
    g.fillRoundedRectangle(603, 81, 57, 58, 12);
    if (model == 3)
    {
        g.setColour(background);
        g.fillRoundedRectangle(613, 89, 8, 42, 3);
        g.fillRoundedRectangle(637, 89, 8, 42, 3);
    }
    for (int i = 0; i < 18; ++i)
    {
        const float x = 275.f + 315.f * (1.f - std::pow(.89f, float(i))) / (1.f - std::pow(.89f, 18.f));
        g.setColour(muted.withAlpha(.65f));
        g.drawLine(x, 92, x, 128, .7f);
    }
    g.setColour(juce::Colour(0xffc9bcb0));
    g.fillRoundedRectangle(139, 91, 12, 42, 2);
    if (model == 1)
    {
        g.setColour(juce::Colour(0xff10191e));
        g.fillRoundedRectangle(186, 90, 15, 43, 3);
        g.fillRoundedRectangle(228, 90, 21, 43, 3);
    }
    if (model == 0)
    {
        g.setColour(accent(0).withAlpha(.3f));
        g.drawEllipse(83, 64, 91, 112, 1);
        g.drawEllipse(108, 83, 42, 70, 1);
    }
    for (int s = 0; s < 6; ++s)
    {
        const float y = 98.f + float(s) * 5;
        juce::Path string;
        string.startNewSubPath(145, y);
        string.quadraticTo(370, y + std::sin(float(s) * 1.4f) * std::min(4.f, peak * 100.f), 652, y);
        g.setColour(model == 3 && s > 2 ? juce::Colour(0xfff4e5cc) : juce::Colour(0xffbac7cd));
        g.strokePath(string, juce::PathStrokeType(.5f + float(s) * .09f));
        g.fillEllipse(644, y - 2, 8, 4);
    }
}
} // namespace
class PhysicalModelGuitarAudioProcessorEditor::Canvas final : public juce::Component
{
  public:
    explicit Canvas(PhysicalModelGuitarAudioProcessor &p)
        : processor(p), modelAttachment(
                            *p.parameters.getParameter("model"),
                            [this](float v)
                            {
                                chosen = juce::jlimit(0, 3, int(std::round(v)));
                                refresh();
                            },
                            &p.undo),
          calibrationAttachment(*p.parameters.getParameter("realCalibration"),
                [this](float v) {
                    calibratedOn = v > .5f;
                    calibrated.selected = calibratedOn;
                    calibrated.repaint();
                }, &p.undo),
          physicsAttachment(*p.parameters.getParameter("physics06"),
                [this](float v) {
                    physicsOn = v > .5f;
                    physical.selected = physicsOn;
                    physical.repaint();
                }, &p.undo),
          physics07Attachment(*p.parameters.getParameter("physics07"),
                [this](float v) {
                    physics07On = v > .5f;
                    physical07.selected = physics07On;
                    physical07.repaint();
                }, &p.undo),
          physics08Attachment(*p.parameters.getParameter("physics08"),
                [this](float v) {
                    physics08On = v > .5f;
                    physical08.selected = physics08On;
                    physical08.repaint();
                }, &p.undo),
          physics09Attachment(*p.parameters.getParameter("physics09"),
                [this](float v) {
                    physics09On = v > .5f;
                    physical09.selected = physics09On;
                    physical09.repaint();
                }, &p.undo),
          physics12Attachment(*p.parameters.getParameter("physics12"),
                [this](float v) {
                    physics12On = v > .5f;
                    physical12.selected=physics12On;
                    physical12.repaint();
                    diagnostic.setEnabled(physics12On);
                    diagDirect.setEnabled(physics12On && diagnosticOn);
                    diagBridge.setEnabled(physics12On && diagnosticOn);
                    diagBody.setEnabled(physics12On && diagnosticOn);
                }, &p.undo),
          physics11Attachment(*p.parameters.getParameter("physics11"),
                [this](float v) {
                    physics11On = v > .5f;
                    physical11.selected = physics11On;
                    physical11.repaint();
                }, &p.undo),
          physics10Attachment(*p.parameters.getParameter("physics10"),
                [this](float v) {
                    physics10On = v > .5f;
                    physical10.selected = physics10On;
                    physical10.repaint();
                }, &p.undo),
          magnetic13Attachment(*p.parameters.getParameter("magnetic13"),
                [this](float v) { magnetic13On=v>.5f; magnetic13.selected=magnetic13On;
                                  magnetic13.repaint(); }, &p.undo),
          radiation13Attachment(*p.parameters.getParameter("radiation13"),
                [this](float v) { radiation13On=v>.5f; radiation13.selected=radiation13On;
                                  radiation13.repaint(); }, &p.undo),
          magnetic14Attachment(*p.parameters.getParameter("magnetic14"),
                [this](float v){magnetic14On=v>.5f;magnetic14.selected=magnetic14On;magnetic14.repaint();}, &p.undo),
          magnetic15Attachment(*p.parameters.getParameter("magnetic15"),
                [this](float v){magnetic15On=v>.5f;magnetic15.selected=magnetic15On;magnetic15.repaint();}, &p.undo),
          directPhase15Attachment(*p.parameters.getParameter("directPhase15"),
                [this](float v){directPhase15Value=v;directPhase15.setCaption("1.5 AIR DELAY: "+juce::String(v,2));directPhase15.repaint();},&p.undo),
          radiation15Attachment(*p.parameters.getParameter("radiation15"),
                [this](float v){radiation15On=v>.5f;radiation15.selected=radiation15On;radiation15.repaint();}, &p.undo),
          radiation16Attachment(*p.parameters.getParameter("radiation16"),
                [this](float v){radiation16On=v>.5f;radiation16.selected=radiation16On;radiation16.repaint();}, &p.undo),
          lateBody16Attachment(*p.parameters.getParameter("lateBody16"),
                [this](float v){lateBody16Value=v;lateBody16.setCaption("1.6 TAIL: "+juce::String(v,2));lateBody16.repaint();}, &p.undo),
          radiation17Attachment(*p.parameters.getParameter("radiation17"),
                [this](float v){radiation17On=v>.5f;radiation17.selected=radiation17On;radiation17.repaint();}, &p.undo),
          phase17Attachment(*p.parameters.getParameter("directBodyPhase17"),
                [this](float v){phase17Value=v;phase17.setCaption("1.7 PHASE: "+juce::String(v,2));phase17.repaint();},&p.undo),
          mix17Attachment(*p.parameters.getParameter("directBodyMix17"),
                [this](float v){mix17Value=v;mix17.setCaption("1.7 DIRECT: "+juce::String(v,2));mix17.repaint();},&p.undo),
          radiation14Attachment(*p.parameters.getParameter("radiation14"),
                [this](float v){radiation14On=v>.5f;radiation14.selected=radiation14On;radiation14.repaint();}, &p.undo),
          measured08Attachment(*p.parameters.getParameter("measuredBridge08"),
                [this](float v) {
                    measured08On = v > .5f;
                    measured08.selected = measured08On;
                    measured08.repaint();
                }, &p.undo),
          diagnosticAttachment(*p.parameters.getParameter("diagnostic171"),
                [this](float v){ diagnosticOn=v>.5f; diagnostic.selected=diagnosticOn;
                                 diagnostic.repaint();
                                 diagDirect.setEnabled(physics12On && diagnosticOn);
                                 diagBridge.setEnabled(physics12On && diagnosticOn);
                                 diagBody.setEnabled(physics12On && diagnosticOn); }, &p.undo),
          diagnosticDirectAttachment(*p.parameters.getParameter("diagnosticDirect171"),
                [this](float v){ diagnosticDirectOn=v>.5f; diagDirect.selected=diagnosticDirectOn;
                                 diagDirect.repaint(); }, &p.undo),
          diagnosticBridgeAttachment(*p.parameters.getParameter("diagnosticBridge171"),
                [this](float v){ diagnosticBridgeOn=v>.5f; diagBridge.selected=diagnosticBridgeOn;
                                 diagBridge.repaint(); }, &p.undo),
          diagnosticBodyAttachment(*p.parameters.getParameter("diagnosticBody171"),
                [this](float v){ diagnosticBodyOn=v>.5f; diagBody.selected=diagnosticBodyOn;
                                 diagBody.repaint(); }, &p.undo),
          undo("UNDO", [&p] { p.undo.undo(); }), redo("REDO", [&p] { p.undo.redo(); }),
          comparison("RESET DEFAULTS", [this] { processor.loadComparisonPreset(chosen); }),
          calibrated("REAL RECORDING CALIBRATION: A/B", [this] {
              calibrationAttachment.setValueAsCompleteGesture(calibratedOn ? 0.f : 1.f);
          }),
          physical("PHYSICS 0.6 / 0.5: A/B", [this] {
              physicsAttachment.setValueAsCompleteGesture(physicsOn ? 0.f : 1.f);
          }),
          physical07("PHYSICS 0.7 / 0.6: A/B", [this] {
              physics07Attachment.setValueAsCompleteGesture(physics07On ? 0.f : 1.f);
          }),
          physical08("BRIDGE 0.8 / 0.7: A/B", [this] {
              physics08Attachment.setValueAsCompleteGesture(physics08On ? 0.f : 1.f);
          }),
          physical09("BRIDGE 0.9 TWO-PORT / 0.8: A/B", [this] {
              physics09Attachment.setValueAsCompleteGesture(physics09On ? 0.f : 1.f);
          }),
          physical12("1.2 UNIFIED / 1.1 HYBRID: A/B", [this] {
              physics12Attachment.setValueAsCompleteGesture(physics12On ? 0.f : 1.f);
          }),
          physical11("1.1 CONTACT / AXIAL: A/B", [this] {
              physics11Attachment.setValueAsCompleteGesture(physics11On ? 0.f : 1.f);
          }),
          physical10("POWER-WAVE 1.0 / 0.9: A/B", [this] {
              physics10Attachment.setValueAsCompleteGesture(physics10On ? 0.f : 1.f);
          }),
          magnetic13("1.3 MAGNETIC COIL: A/B", [this] {
              magnetic13Attachment.setValueAsCompleteGesture(magnetic13On?0.f:1.f);
          }),
          radiation13("1.3 NYLON RADIATION: A/B", [this] {
              radiation13Attachment.setValueAsCompleteGesture(radiation13On?0.f:1.f);
          }),
          magnetic14("1.4 MAGNETIC ATTACK / 1.3: A/B", [this] {
              magnetic14Attachment.setValueAsCompleteGesture(magnetic14On?0.f:1.f);
          }),
          radiation14("1.4 NYLON HIGH BAND / 1.3: A/B", [this] {
              radiation14Attachment.setValueAsCompleteGesture(radiation14On?0.f:1.f);
          }),
          magnetic15("1.5 MEASURED COIL LOAD: A/B",[this]{
              magnetic15Attachment.setValueAsCompleteGesture(magnetic15On?0.f:1.f);
          }),
          directPhase15("1.5 AIR PATH DELAY",[this]{
              const float v=directPhase15Value<.25f?.5f:directPhase15Value<.75f?1.f:0.f;
              directPhase15Attachment.setValueAsCompleteGesture(v);
          }),
          radiation15("1.5 MEASURED NYLON PHASE: A/B",[this]{
              radiation15Attachment.setValueAsCompleteGesture(radiation15On?0.f:1.f);
          }),
          radiation16("1.6 LONG NYLON MODES / 1.5: A/B",[this]{
              radiation16Attachment.setValueAsCompleteGesture(radiation16On?0.f:1.f);
          }),
          lateBody16("1.6 LATE TAIL MIX",[this]{
              const float v=lateBody16Value<.25f?.5f:lateBody16Value<.75f?1.f:0.f;
              lateBody16Attachment.setValueAsCompleteGesture(v);
          }),
          radiation17("1.7 DIRECT/BODY PHASE: A/B",[this]{
              radiation17Attachment.setValueAsCompleteGesture(radiation17On?0.f:1.f);
          }),
          phase17("1.7 RELATIVE DELAY",[this]{
              const float v=phase17Value<.20f?.5f:phase17Value<.60f?.9f:.1f;
              phase17Attachment.setValueAsCompleteGesture(v);
          }),
          mix17("1.7 DIRECT MIX",[this]{
              const float v=mix17Value<.25f?.5f:mix17Value<.75f?1.f:0.f;
              mix17Attachment.setValueAsCompleteGesture(v);
          }),
          measured08("ZENODO CLASSICAL BRIDGE: A/B", [this] {
              measured08Attachment.setValueAsCompleteGesture(measured08On ? 0.f : 1.f);
          }),
          diagnostic("1.7.1 UNIFIED DIAGNOSTICS: A/B", [this] {
              diagnosticAttachment.setValueAsCompleteGesture(diagnosticOn?0.f:1.f);
          }),
          diagDirect("STRING / PICKUP DIRECT", [this] {
              diagnosticDirectAttachment.setValueAsCompleteGesture(diagnosticDirectOn?0.f:1.f);
          }),
          diagBridge("PHYSICAL BRIDGE COUPLING", [this] {
              diagnosticBridgeAttachment.setValueAsCompleteGesture(diagnosticBridgeOn?0.f:1.f);
          }),
          diagBody("BRIDGE / BODY RADIATION", [this] {
              diagnosticBodyAttachment.setValueAsCompleteGesture(diagnosticBodyOn?0.f:1.f);
          })
    {
        for (int i = 0; i < 4; ++i)
        {
            models[size_t(i)] = std::make_unique<Action>(
                pmg::modelNames[i], [this, i] { modelAttachment.setValueAsCompleteGesture(float(i)); });
            addAndMakeVisible(*models[size_t(i)]);
        }
        const char *ids[] = {"decay", "brightness", "pluckPosition", "pickHardness",
                             "pickupPosition", "bodyResonance", "bodySize", "outputLevel",
                             "material", "pickupCharacter", "bridgeCoupling", "touch",
                             "pickAngle", "fretClearance", "pickupAperture", "bridgeMobility"};
        const char *names[] = {"DECAY", "BRIGHTNESS", "PLUCK POINT", "PICK HARDNESS",
                               "PICKUP POINT", "BODY RESONANCE", "BODY SIZE", "OUTPUT LEVEL",
                               "STRING MATERIAL", "PICKUP CHARACTER", "BRIDGE COUPLING", "FINGER CONTACT",
                               "PICK ANGLE", "FRET CLEARANCE", "PICKUP APERTURE", "BRIDGE MOBILITY"};
        const char *hints[] = {
            "Decay seconds", "Loss / harmonics", "Plucking position", "Pick slip stiffness",
            "Coil position", "Radiation amount", "Prepare / body scale", "Output gain",
            "String dispersion", "Double-coil spacing", "String/body feedback", "Fretting pressure",
            "Polarization split", "Action / buzz threshold", "Finite pickup width", "Bridge admittance"};
        for (size_t i = 0; i < dials.size(); ++i)
        {
            dials[i] = std::make_unique<Dial>(p, ids[i], names[i], hints[i]);
            addAndMakeVisible(*dials[i]);
        }
        addAndMakeVisible(undo);
        addAndMakeVisible(redo);
        addAndMakeVisible(comparison);
        addAndMakeVisible(calibrated);
        addAndMakeVisible(physical);
        addAndMakeVisible(physical07);
        addAndMakeVisible(physical08);
        addAndMakeVisible(physical09);
        addAndMakeVisible(physical10);
        addAndMakeVisible(physical11);
        addAndMakeVisible(physical12);
        addAndMakeVisible(measured08);
        addAndMakeVisible(magnetic13);
        addAndMakeVisible(radiation13);
        addAndMakeVisible(magnetic14);
        addAndMakeVisible(radiation14);
        addAndMakeVisible(magnetic15);
        addAndMakeVisible(radiation15);
        addAndMakeVisible(directPhase15);
        addAndMakeVisible(radiation16);
        addAndMakeVisible(lateBody16);
        addAndMakeVisible(radiation17);
        addAndMakeVisible(phase17);
        addAndMakeVisible(mix17);
        addAndMakeVisible(diagnostic);
        addAndMakeVisible(diagDirect);
        addAndMakeVisible(diagBridge);
        addAndMakeVisible(diagBody);
        modelAttachment.sendInitialUpdate();
        calibrationAttachment.sendInitialUpdate();
        physicsAttachment.sendInitialUpdate();
        physics07Attachment.sendInitialUpdate();
        physics08Attachment.sendInitialUpdate();
        physics09Attachment.sendInitialUpdate();
        physics10Attachment.sendInitialUpdate();
        physics11Attachment.sendInitialUpdate();
        physics12Attachment.sendInitialUpdate();
        measured08Attachment.sendInitialUpdate();
        magnetic13Attachment.sendInitialUpdate();
        radiation13Attachment.sendInitialUpdate();
        magnetic14Attachment.sendInitialUpdate();
        radiation14Attachment.sendInitialUpdate();
        magnetic15Attachment.sendInitialUpdate();
        radiation15Attachment.sendInitialUpdate();
        directPhase15Attachment.sendInitialUpdate();
        radiation16Attachment.sendInitialUpdate();
        lateBody16Attachment.sendInitialUpdate();
        radiation17Attachment.sendInitialUpdate();
        phase17Attachment.sendInitialUpdate();
        mix17Attachment.sendInitialUpdate();
        diagnosticAttachment.sendInitialUpdate();
        diagnosticDirectAttachment.sendInitialUpdate();
        diagnosticBridgeAttachment.sendInitialUpdate();
        diagnosticBodyAttachment.sendInitialUpdate();
    }
    void refresh()
    {
        for (int i = 0; i < 4; ++i)
            if (models[size_t(i)])
            {
                models[size_t(i)]->selected = chosen == i;
                models[size_t(i)]->colour = accent(i);
                models[size_t(i)]->repaint();
            }
        for (auto &d : dials)
            if (d)
            {
                d->colour = accent(chosen);
                d->setEnabled(true);
                d->repaint();
            }
        // All four models use the same physical mechanisms; the electric
        // pickup controls are still meaningful only for the magnetic model.
        dials[4]->setEnabled(chosen == 1);
        dials[9]->setEnabled(chosen == 1);
        dials[14]->setEnabled(chosen == 1);
        measured08.setEnabled(chosen == 3);
        magnetic13.setEnabled(chosen == 1);
        radiation13.setEnabled(chosen == 3);
        magnetic14.setEnabled(chosen == 1);
        radiation14.setEnabled(chosen == 3);
        magnetic15.setEnabled(chosen == 1);
        radiation15.setEnabled(chosen == 3);
        directPhase15.setEnabled(chosen == 3);
        radiation16.setEnabled(chosen == 3);
        lateBody16.setEnabled(chosen == 3);
        radiation17.setEnabled(chosen == 3);
        phase17.setEnabled(chosen == 3);
        mix17.setEnabled(chosen == 3);
        diagnostic.setEnabled(physics12On);
        diagDirect.setEnabled(physics12On && diagnosticOn);
        diagBridge.setEnabled(physics12On && diagnosticOn);
        diagBody.setEnabled(physics12On && diagnosticOn);
        repaint();
    }
    void paint(juce::Graphics &g) override
    {
        g.fillAll(background);
        text(g, "PHYSICAL MODEL GUITAR 1.7.1 DIAGNOSTIC", {24, 16, 620, 34}, 25, ink);
        text(g, "EARLY MEASURED NYLON PRESSURE + LONG SOUND BOARD MODES", {25, 52, 620, 17}, 11, muted);
        for (int group = 0; group < 4; ++group)
        {
            const int x = 20 + group * 245;
            g.setColour(panel);
            g.fillRoundedRectangle(float(x), 369, 231, 330, 12);
            text(g,
                 group == 0 ? "01  STRING & EXCITATION"
                 : group == 1 ? "02  BODY & OUTPUT"
                 : group == 2 ? "03  CORE PHYSICS"
                              : "04  CONTACT / COUPLING",
                 {x + 11, 382, 215, 22}, 11, accent(chosen));
        }
        g.setColour(panel);
        g.fillRoundedRectangle(20, 158, 960, 190, 12);
        guitar(g, chosen, {30, 163, 650, 179}, processor.peak.load());
        text(g, pmg::modelNames[chosen], {697, 184, 260, 31}, 22, accent(chosen));
        const char *summaries[] = {"Coupled dual-plane waveguides", "Magnetic aperture + RLC-like load",
                                   "Steel string + 10 soundboard modes",
                                   "Nylon string + compliant contact"};
        text(g, summaries[chosen], {697, 221, 260, 28}, 11, muted);
        const bool pending = processor.modelTransition.load() || processor.appliedModel.load() != chosen;
        text(g, pending ? "TRANSITION / 40 ms" : "APPLIED / " + juce::String(pmg::modelNames[chosen]),
             {697, 260, 260, 23}, 11, ink);
        text(g, juce::String(processor.voiceCount.load()) + " / 6 STRINGS  |  MIDI IN", {697, 288, 260, 20},
             11, muted);
        text(g, calibratedOn ? "CALIBRATION: ON" : "CALIBRATION: OFF", {697, 143, 135, 14}, 10, calibratedOn ? accent(chosen) : muted);
        text(g, physics07On ? "PHYSICS 0.7: ON" : "PHYSICS 0.6: OFF", {826, 143, 140, 14}, 10, physics07On ? accent(chosen) : muted);
        text(g, "Drag vertically / Shift: fine / Wheel or arrows / Double-click: reset", {24, 839, 645, 18},
             11, muted);
        text(g,
             "1.7: separate string/bridge readouts with tunable coherent air delay; unified mode required.",
             {24, 855, 950, 15}, 10, muted);
        text(g, "DIAGNOSTIC / RAW 1.2 UNIFIED TAPS  (OFF = bit-identical v1.7)",
             {24, 880, 950, 17}, 12, accent(chosen));
        text(g, "Bridge OFF: rigid passive reflection; toggling absorbs old bridge tail. Retrigger note after A/B.",
             {24, 953, 950, 15}, 10, muted);
    }
    void resized() override
    {
        for (int i = 0; i < 4; ++i)
            models[size_t(i)]->setBounds(20 + i * 245, 88, 230, 52);
        for (int i = 0; i < 16; ++i)
        {
            const int group = i / 4, cell = i % 4;
            dials[size_t(i)]->setBounds(27 + group * 245 + (cell % 2) * 111,
                                        411 + (cell / 2) * 137, 108, 126);
        }
        undo.setBounds(798, 24, 82, 32);
        redo.setBounds(890, 24, 82, 32);
        comparison.setBounds(644, 24, 144, 32);
        calibrated.setBounds(697, 302, 260, 23);
        physical.setBounds(697, 325, 260, 23);
        physical07.setBounds(697, 348, 260, 20);
        physical10.setBounds(697, 373, 260, 24);
        physical11.setBounds(390, 24, 245, 32);
        physical12.setBounds(697, 274, 260, 24);
        physical09.setBounds(697, 676, 260, 25);
        physical08.setBounds(697, 701, 260, 25);
        measured08.setBounds(697, 728, 260, 25);
        magnetic13.setBounds(27, 702, 313, 26);
        radiation13.setBounds(354, 702, 313, 26);
        magnetic14.setBounds(27, 737, 313, 26);
        radiation14.setBounds(354, 737, 313, 26);
        magnetic15.setBounds(27, 767, 313, 26);
        radiation15.setBounds(354, 767, 313, 26);
        directPhase15.setBounds(697, 767, 260, 26);
        radiation16.setBounds(354, 798, 313, 26);
        lateBody16.setBounds(697, 798, 260, 26);
        radiation17.setBounds(27, 830, 313, 26);
        phase17.setBounds(354, 830, 313, 26);
        mix17.setBounds(697, 830, 260, 26);
        diagnostic.setBounds(27, 898, 946, 24);
        diagDirect.setBounds(27, 925, 307, 24);
        diagBridge.setBounds(345, 925, 307, 24);
        diagBody.setBounds(663, 925, 310, 24);
    }
    bool fits() const
    {
        for (auto &d : dials)
            if (!getLocalBounds().contains(d->getBounds()))
                return false;
        return true;
    }
    std::array<std::unique_ptr<Dial>, 16> dials;

  private:
    PhysicalModelGuitarAudioProcessor &processor;
    int chosen = 0;
    float directPhase15Value=.5f,lateBody16Value=.5f,phase17Value=.5f,mix17Value=.5f;
    bool diagnosticOn=false,diagnosticDirectOn=true,diagnosticBridgeOn=true,diagnosticBodyOn=true;
    bool calibratedOn = false, physicsOn = true, physics07On = true, physics08On = true, physics09On = true, physics10On = true, physics11On = true, physics12On = false, measured08On = false, magnetic13On = false, radiation13On = false, magnetic14On = false, radiation14On = false, magnetic15On = false, radiation15On = false, radiation16On = false,radiation17On = false;
    juce::ParameterAttachment modelAttachment, calibrationAttachment, physicsAttachment, physics07Attachment, physics08Attachment, physics09Attachment, physics10Attachment, physics11Attachment, physics12Attachment, measured08Attachment, magnetic13Attachment, radiation13Attachment, magnetic14Attachment, radiation14Attachment, magnetic15Attachment, radiation15Attachment, directPhase15Attachment, radiation16Attachment, lateBody16Attachment, radiation17Attachment, phase17Attachment, mix17Attachment, diagnosticAttachment, diagnosticDirectAttachment, diagnosticBridgeAttachment, diagnosticBodyAttachment;
    std::array<std::unique_ptr<Action>, 4> models;
    Action undo, redo, comparison, calibrated, physical, physical07, physical08, physical09, physical10, physical11, physical12, measured08, magnetic13, radiation13, magnetic14, radiation14, magnetic15, radiation15, directPhase15, radiation16, lateBody16, radiation17, phase17, mix17, diagnostic, diagDirect, diagBridge, diagBody;
};
PhysicalModelGuitarAudioProcessorEditor::PhysicalModelGuitarAudioProcessorEditor(
    PhysicalModelGuitarAudioProcessor &p)
    : AudioProcessorEditor(&p), processor(p)
{
    canvas = std::make_unique<Canvas>(p);
    addAndMakeVisible(*canvas);
    setResizable(true, true);
    setResizeLimits(900, 882, 1800, 1764);
    getConstrainer()->setFixedAspectRatio(1000. / 980.);
    setSize(p.editorWidth.load(), p.editorHeight.load());
    startTimerHz(24);
}
PhysicalModelGuitarAudioProcessorEditor::~PhysicalModelGuitarAudioProcessorEditor()
{
    stopTimer();
}
void PhysicalModelGuitarAudioProcessorEditor::paint(juce::Graphics &g)
{
    g.fillAll(background);
}
void PhysicalModelGuitarAudioProcessorEditor::resized()
{
    if (canvas)
    {
        const float scale = std::min(float(getWidth()) / 1000.f, float(getHeight()) / 980.f);
        canvas->setBounds(0, 0, 1000, 980);
        canvas->setTransform(juce::AffineTransform::scale(scale));
        processor.editorWidth.store(getWidth());
        processor.editorHeight.store(getHeight());
    }
}
void PhysicalModelGuitarAudioProcessorEditor::timerCallback()
{
    canvas->repaint();
}
int PhysicalModelGuitarAudioProcessorEditor::getControlParameterIndex(juce::Component &c)
{
    for (auto &d : canvas->dials)
        if (d.get() == &c)
            return d->param.getParameterIndex();
    return -1;
}
bool PhysicalModelGuitarAudioProcessorEditor::controlsFit() const
{
    return canvas->fits();
}