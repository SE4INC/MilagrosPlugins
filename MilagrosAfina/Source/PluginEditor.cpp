#include "PluginEditor.h"
#include <algorithm>

namespace
{
    const juce::Colour bgColour    (0xff14161c);
    const juce::Colour panelColour (0xff1d2029);
    const juce::Colour accent      (0xffffb347);
    const juce::Colour cyan        (0xff7ad7f0);

    void setFloat (juce::AudioProcessorValueTreeState& s, const char* id, float v)
    {
        if (auto* p = dynamic_cast<juce::AudioParameterFloat*> (s.getParameter (id)))
        {
            p->beginChangeGesture();
            *p = v;
            p->endChangeGesture();
        }
    }

    void setChoice (juce::AudioProcessorValueTreeState& s, const char* id, int idx)
    {
        if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (s.getParameter (id)))
        {
            p->beginChangeGesture();
            *p = idx;
            p->endChangeGesture();
        }
    }

    juce::String keyText (int root, bool minor)
    {
        return juce::String (afina::noteNames (root)) + (minor ? es (" menor") : es (" mayor"));
    }
}

//==============================================================================
// Pantalla de notas: lo que cantas (celeste) y lo que sale ya afinado (naranja)
class MilagrosAfinaEditor::PitchView : public juce::Component
{
public:
    explicit PitchView (MilagrosAfinaProcessor& p) : proc (p) {}

    void paint (juce::Graphics& g) override
    {
        constexpr int N = MilagrosAfinaProcessor::dispSize;
        auto r = getLocalBounds().toFloat();
        g.setColour (panelColour);
        g.fillRoundedRectangle (r, 8.0f);

        std::array<float, N> in {}, out {};
        proc.copyDisplay (in, out);

        std::vector<float> v;
        for (float m : in)
            if (m > 0.0f)
                v.push_back (m);
        if (v.size() > 8)
        {
            std::nth_element (v.begin(), v.begin() + (long) (v.size() / 2), v.end());
            viewCenter += (v[v.size() / 2] - viewCenter) * 0.08f;
        }

        const float semis = 20.0f;
        const float lo = viewCenter - semis * 0.5f;
        auto yOf = [&] (float midi) { return r.getBottom() - (midi - lo) / semis * r.getHeight(); };

        const int root = (int) proc.apvts.getRawParameterValue ("key")->load();
        const int scale = (int) proc.apvts.getRawParameterValue ("scale")->load();

        g.setFont (11.0f);
        for (int n = (int) std::ceil (lo); n <= (int) std::floor (lo + semis); ++n)
        {
            const float y = yOf ((float) n);
            const bool allowed = afina::inScale (scale, root, ((n % 12) + 12) % 12);
            g.setColour (allowed ? juce::Colour (0x40ffffff) : juce::Colour (0x12ffffff));
            g.drawHorizontalLine ((int) y, r.getX(), r.getRight());
            if (allowed)
            {
                g.setColour (juce::Colours::lightgrey);
                g.drawText (afina::noteWithOctave (n), (int) r.getX() + 4, (int) y - 14, 50, 14, juce::Justification::left);
            }
        }

        auto drawLine = [&] (const std::array<float, N>& data, juce::Colour c, float thick)
        {
            juce::Path path;
            bool pen = false;
            for (int i = 0; i < N; ++i)
            {
                if (data[(size_t) i] <= 0.0f)
                {
                    pen = false;
                    continue;
                }
                const float x = r.getX() + (float) i / (float) (N - 1) * r.getWidth();
                const float y = juce::jlimit (r.getY(), r.getBottom(), yOf (data[(size_t) i]));
                if (! pen)
                {
                    path.startNewSubPath (x, y);
                    pen = true;
                }
                else
                {
                    path.lineTo (x, y);
                }
            }
            g.setColour (c);
            g.strokePath (path, juce::PathStrokeType (thick));
        };
        drawLine (in, cyan, 1.8f);
        drawLine (out, accent, 2.4f);

        // Nota actual
        const auto live = proc.getLive();
        juce::String text = es ("Canta para ver tu nota…");
        if (live.voiced)
        {
            const int nIn = (int) std::lround (live.inMidi);
            const int cents = juce::roundToInt ((live.inMidi - (float) nIn) * 100.0f);
            text = es ("Cantas: ") + afina::noteWithOctave (nIn) + " (" + (cents >= 0 ? "+" : "")
                 + juce::String (cents) + es (" c)   →   Afina a: ") + afina::noteWithOctave ((int) live.targetMidi);
        }
        g.setColour (juce::Colours::white);
        g.setFont (15.0f);
        g.drawText (text, getLocalBounds().removeFromTop (26).reduced (10, 0), juce::Justification::centredRight);

        g.setColour (juce::Colours::grey);
        g.setFont (11.0f);
        g.drawText (es ("celeste = tu voz   ·   naranja = voz afinada"),
                    getLocalBounds().removeFromBottom (18).reduced (10, 0), juce::Justification::centredRight);
    }

private:
    MilagrosAfinaProcessor& proc;
    float viewCenter = 60.0f;
};

//==============================================================================
// PÁGINA 1: afinar
class MilagrosAfinaEditor::MainPage : public juce::Component
{
public:
    explicit MainPage (MilagrosAfinaProcessor& p) : proc (p), pitchView (p)
    {
        setupCombo (keyBox, keyAtt, "key");
        setupCombo (scaleBox, scaleAtt, "scale");

        styleBox.addItem (es ("Estilo rápido (elige uno)"), 1);
        styleBox.addItem (es ("Natural (casi no se nota)"), 2);
        styleBox.addItem (es ("Pop moderno"), 3);
        styleBox.addItem (es ("Trap / efecto robótico"), 4);
        styleBox.addItem (es ("Retoque suave"), 5);
        styleBox.setSelectedId (1, juce::dontSendNotification);
        styleBox.onChange = [this] { applyStyle (styleBox.getSelectedId()); };
        addAndMakeVisible (styleBox);

        addAndMakeVisible (pitchView);

        setupKnob (speed,  "speed",  es ("Velocidad"));
        setupKnob (amount, "amount", es ("Cantidad"));
        setupKnob (refA,   "refA",   es ("Afinación base"));
        setupKnob (thresh, "thresh", es ("Umbral de voz"));
        amount.slider.setColour (juce::Slider::rotarySliderFillColourId, accent);
    }

    void refresh() { pitchView.repaint(); }

    void paint (juce::Graphics& g) override
    {
        g.setColour (juce::Colours::grey);
        g.setFont (12.0f);
        g.drawText (es ("MilagrosAfina v0.1 · gratis y de código abierto"),
                    getLocalBounds().removeFromBottom (20), juce::Justification::centred);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (12);
        r.removeFromBottom (20);

        auto top = r.removeFromTop (30);
        keyBox.setBounds (top.removeFromLeft (130));
        top.removeFromLeft (8);
        scaleBox.setBounds (top.removeFromLeft (200));
        top.removeFromLeft (8);
        styleBox.setBounds (top);

        r.removeFromTop (8);
        pitchView.setBounds (r.removeFromTop (240));
        r.removeFromTop (6);

        std::vector<Knob*> ks { &speed, &amount, &refA, &thresh };
        const int w = r.getWidth() / (int) ks.size();
        for (auto* k : ks)
        {
            auto cell = r.removeFromLeft (w).reduced (4);
            k->label.setBounds (cell.removeFromTop (20));
            k->slider.setBounds (cell);
        }
    }

private:
    struct Knob
    {
        juce::Slider slider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    void setupKnob (Knob& k, const char* id, const juce::String& text)
    {
        k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 90, 18);
        k.slider.setColour (juce::Slider::rotarySliderFillColourId, cyan);
        k.label.setText (text, juce::dontSendNotification);
        k.label.setJustificationType (juce::Justification::centred);
        k.label.setColour (juce::Label::textColourId, juce::Colours::white);
        addAndMakeVisible (k.slider);
        addAndMakeVisible (k.label);
        k.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, id, k.slider);
    }

    void setupCombo (juce::ComboBox& box,
                     std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>& att,
                     const char* id)
    {
        if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (proc.apvts.getParameter (id)))
            box.addItemList (p->choices, 1);
        addAndMakeVisible (box);
        att = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, id, box);
    }

    void applyStyle (int id)
    {
        switch (id)
        {
            case 2: setFloat (proc.apvts, "speed", 120.0f); setFloat (proc.apvts, "amount", 0.70f); break;
            case 3: setFloat (proc.apvts, "speed", 35.0f);  setFloat (proc.apvts, "amount", 0.90f); break;
            case 4: setFloat (proc.apvts, "speed", 0.0f);   setFloat (proc.apvts, "amount", 1.00f); break;
            case 5: setFloat (proc.apvts, "speed", 200.0f); setFloat (proc.apvts, "amount", 0.50f); break;
            default: break;
        }
    }

    MilagrosAfinaProcessor& proc;
    PitchView pitchView;
    juce::ComboBox keyBox, scaleBox, styleBox;
    Knob speed, amount, refA, thresh;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> keyAtt, scaleAtt;
};

//==============================================================================
// PÁGINA 2: la hoja de consejos (Asesor)
namespace
{
    struct Advice
    {
        juce::String text;
        int voiceRoot = -1, instRoot = -1;
        bool voiceMinor = false, instMinor = false;
        float speed = -1.0f, amount = -1.0f;
    };

    Advice buildAdvice (const MilagrosAfinaProcessor::Analysis& a)
    {
        Advice r;

        if (a.voicedFrames < 400)
        {
            r.text = es ("Canta o habla unos segundos (mejor la parte principal de tu tema) con el plugin en la pista de tu voz.\n\n"
                         "Aquí aparecerán consejos: tu rango de voz, en qué tonalidad cantas, qué tan afinado estás y qué ajustes te convienen.");
            return r;
        }

        int total = 0;
        for (int c : a.pitchHist)
            total += c;
        auto pct = [&] (float p)
        {
            const int goal = (int) ((float) total * p);
            int acc = 0;
            for (int m = 0; m < 128; ++m)
            {
                acc += a.pitchHist[(size_t) m];
                if (acc >= goal)
                    return m;
            }
            return 0;
        };
        const int p5 = pct (0.05f), p50 = pct (0.5f), p95 = pct (0.95f);

        juce::StringArray lines;

        // 1) Rango de voz
        const char* tipo = p50 < 50 ? "grave" : p50 < 57 ? "media-grave" : p50 < 64 ? "media-aguda" : "aguda";
        lines.add (es ("• TU VOZ: cantas entre ") + afina::noteWithOctave (p5) + es (" y ") + afina::noteWithOctave (p95)
                   + es (", y tu nota más cómoda es ") + afina::noteWithOctave (p50)
                   + es (". Es una voz ") + es (tipo) + es ("."));

        // 2) Tonalidad según tu voz
        const auto vk = KeyAnalyzer::estimate (a.voiceChroma);
        if (vk.root >= 0)
        {
            r.voiceRoot = vk.root;
            r.voiceMinor = vk.minor;
            const int conf = juce::jlimit (0, 100, juce::roundToInt (vk.score * 100.0f));
            lines.add (es ("• TONALIDAD (según lo que cantas): parece ") + keyText (vk.root, vk.minor)
                       + es (" (seguridad ") + juce::String (conf) + es (" %). La tonalidad son las notas \"permitidas\" de la canción. ")
                       + (conf < 55 ? es ("Si no estás seguro, pregunta al productor del beat o usa el botón de la instrumental.")
                                    : es ("Pulsa el botón de abajo para usarla.")));
        }

        // 3) Tonalidad de la instrumental
        if (a.instFrames > 10)
        {
            const auto ik = KeyAnalyzer::estimate (a.instChroma);
            if (ik.root >= 0)
            {
                r.instRoot = ik.root;
                r.instMinor = ik.minor;
                lines.add (es ("• INSTRUMENTAL: parece estar en ") + keyText (ik.root, ik.minor)
                           + es (". Usa ESA tonalidad para afinar tu voz."));

                if (vk.root >= 0)
                {
                    const bool same = ik.root == vk.root && ik.minor == vk.minor;
                    const bool relative = (! ik.minor && vk.minor && vk.root == (ik.root + 9) % 12)
                                       || (ik.minor && ! vk.minor && vk.root == (ik.root + 3) % 12);
                    if (same || relative)
                        lines.add (es ("• Tu voz y la instrumental están en la misma familia de notas. ¡Buen encaje!"));
                    else
                        lines.add (es ("• Tu voz y la instrumental parecen estar en tonalidades distintas. Si la canción suena rara, "
                                       "prueba bajar o subir la instrumental (o cantar otra melodía) hasta que coincidan."));
                }
            }
        }
        else
        {
            lines.add (es ("• Para detectar la tonalidad de la instrumental, envíala al sidechain de este plugin "
                           "(si tu versión de FL Studio lo permite). Si no, elige la tonalidad a mano arriba."));
        }

        // 4) Qué tan afinado estás
        const float inTune = 100.0f * (float) a.inTuneFrames / (float) juce::jmax (1, a.voicedFrames);
        r.speed = inTune > 85.0f ? 150.0f : inTune > 60.0f ? 70.0f : 25.0f;
        r.amount = inTune > 85.0f ? 0.60f : inTune > 60.0f ? 0.85f : 1.00f;
        lines.add (es ("• AFINACIÓN: el ") + juce::String (juce::roundToInt (inTune))
                   + es (" % de tus notas cae a menos de 25 centésimas de la nota correcta (100 centésimas = un semitono). ")
                   + (inTune > 85.0f ? es ("Cantas muy afinado: usa una corrección suave.")
                       : inTune > 60.0f ? es ("Cantas bastante bien: una corrección media basta.")
                                        : es ("Te conviene una corrección más fuerte.")));
        lines.add (es ("   Ajustes recomendados: Velocidad ") + juce::String (juce::roundToInt (r.speed)) + es (" ms, Cantidad ")
                   + juce::String (juce::roundToInt (r.amount * 100.0f)) + es (" %."));

        // 5) Calidad de la señal
        if (a.avgConfidence < 0.6f)
            lines.add (es ("• AVISO: tu voz se detecta poco clara. Reduce el ruido (por ejemplo con MilagrosLimpio antes de este plugin), "
                           "acércate al micrófono y evita el eco del cuarto."));
        if (a.peakDb > -1.0f)
            lines.add (es ("• AVISO: tu señal llega al tope. Baja la ganancia del micrófono: la distorsión confunde al afinador."));
        if (a.avgLevelDb < -40.0f)
            lines.add (es ("• AVISO: tu volumen es bajo. Sube la ganancia de entrada o baja el \"Umbral de voz\" si se pierden notas."));

        lines.add (es ("\nGLOSARIO\nTonalidad: las notas que \"suenan bien\" en tu canción.\n"
                       "Velocidad: qué tan rápido te lleva a la nota (0 = efecto robótico, más alto = más natural).\n"
                       "Cantidad: cuánto se corrige (100 % = todo).\n"
                       "Afinación base: déjala en 440 Hz, salvo que tu instrumental esté desafinada."));

        r.text = lines.joinIntoString ("\n\n");
        return r;
    }
}

class MilagrosAfinaEditor::AdvisorPage : public juce::Component
{
public:
    explicit AdvisorPage (MilagrosAfinaProcessor& p) : proc (p)
    {
        text.setMultiLine (true);
        text.setReadOnly (true);
        text.setCaretVisible (false);
        text.setColour (juce::TextEditor::backgroundColourId, panelColour);
        text.setColour (juce::TextEditor::textColourId, juce::Colours::white);
        text.setColour (juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
        addAndMakeVisible (text);

        useVoiceBtn.setButtonText (es ("Usar tonalidad de mi voz"));
        useInstBtn.setButtonText (es ("Usar tonalidad de la instrumental"));
        applyBtn.setButtonText (es ("Aplicar ajustes recomendados"));
        resetBtn.setButtonText (es ("Reiniciar análisis"));
        for (auto* b : { &useVoiceBtn, &useInstBtn, &applyBtn, &resetBtn })
            addAndMakeVisible (*b);

        useVoiceBtn.onClick = [this]
        {
            if (advice.voiceRoot >= 0)
            {
                setChoice (proc.apvts, "key", advice.voiceRoot);
                setChoice (proc.apvts, "scale", advice.voiceMinor ? 2 : 1);
            }
        };
        useInstBtn.onClick = [this]
        {
            if (advice.instRoot >= 0)
            {
                setChoice (proc.apvts, "key", advice.instRoot);
                setChoice (proc.apvts, "scale", advice.instMinor ? 2 : 1);
            }
        };
        applyBtn.onClick = [this]
        {
            if (advice.speed >= 0.0f)
            {
                setFloat (proc.apvts, "speed", advice.speed);
                setFloat (proc.apvts, "amount", advice.amount);
            }
        };
        resetBtn.onClick = [this] { proc.resetAnalysis(); refresh(); };

        refresh();
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (12);
        auto row2 = r.removeFromBottom (34);
        r.removeFromBottom (6);
        auto row1 = r.removeFromBottom (34);
        r.removeFromBottom (8);

        useVoiceBtn.setBounds (row1.removeFromLeft (row1.getWidth() / 2 - 4));
        row1.removeFromLeft (8);
        useInstBtn.setBounds (row1);
        applyBtn.setBounds (row2.removeFromLeft (row2.getWidth() / 2 - 4));
        row2.removeFromLeft (8);
        resetBtn.setBounds (row2);
        text.setBounds (r);
    }

    void refresh()
    {
        advice = buildAdvice (proc.getAnalysis());
        if (text.getText() != advice.text)
            text.setText (advice.text, false);
        useVoiceBtn.setEnabled (advice.voiceRoot >= 0);
        useInstBtn.setEnabled (advice.instRoot >= 0);
        applyBtn.setEnabled (advice.speed >= 0.0f);
    }

private:
    MilagrosAfinaProcessor& proc;
    juce::TextEditor text;
    juce::TextButton useVoiceBtn, useInstBtn, applyBtn, resetBtn;
    Advice advice;
};

//==============================================================================
MilagrosAfinaEditor::MilagrosAfinaEditor (MilagrosAfinaProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    mainPage = std::make_unique<MainPage> (proc);
    advisorPage = std::make_unique<AdvisorPage> (proc);

    tabs.setTabBarDepth (32);
    tabs.setOutline (0);
    tabs.addTab (es ("Afinar"), panelColour, mainPage.get(), false);
    tabs.addTab (es ("Asesor (hoja de consejos)"), panelColour, advisorPage.get(), false);
    addAndMakeVisible (tabs);

    setSize (760, 520);
    startTimerHz (30);
}

MilagrosAfinaEditor::~MilagrosAfinaEditor()
{
    stopTimer();
}

void MilagrosAfinaEditor::paint (juce::Graphics& g)
{
    g.fillAll (bgColour);
}

void MilagrosAfinaEditor::resized()
{
    tabs.setBounds (getLocalBounds());
}

void MilagrosAfinaEditor::timerCallback()
{
    if (tabs.getCurrentTabIndex() == 0)
    {
        mainPage->refresh();
    }
    else if (++advisorTick >= 10)
    {
        advisorTick = 0;
        advisorPage->refresh();
    }
}
