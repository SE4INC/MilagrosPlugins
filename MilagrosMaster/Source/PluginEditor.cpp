#include "PluginEditor.h"

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

    juce::String fmt (float v, const char* unit)
    {
        return v <= -100.0f ? es ("— ") + unit : juce::String (v, 1) + " " + unit;
    }

    // Estilos: graves, brillo, pegamento, ancho, calidez, intensidad, objetivo
    struct Preset { const char* name; float lows, bright, glue, width, warm, drive; int target; };
    const Preset presets[] = {
        { "Trap",                1.5f, 2.0f, 0.45f, 1.10f, 0.35f, 6.0f, 2 },
        { "Hip hop / Boom bap",  1.0f, 1.0f, 0.50f, 1.00f, 0.40f, 4.5f, 1 },
        { "Drill",               2.0f, 1.5f, 0.50f, 1.05f, 0.35f, 6.0f, 2 },
        { "Reggaeton / Dembow",  1.0f, 2.0f, 0.50f, 1.10f, 0.30f, 6.5f, 2 },
        { "Pop",                 0.0f, 2.0f, 0.40f, 1.15f, 0.25f, 4.0f, 1 },
        { "R&B / Pop suave",     0.5f, 1.0f, 0.35f, 1.10f, 0.30f, 3.0f, 0 },
        { "Rock",                0.5f, 1.0f, 0.55f, 1.05f, 0.30f, 5.0f, 1 },
        { "Acústico / Voz",      0.0f, 0.5f, 0.25f, 1.00f, 0.15f, 1.5f, 4 }
    };
}

//==============================================================================
// Medidores grandes: volumen del tema, volumen ahora, pico y barra hacia el objetivo
class MilagrosMasterEditor::MeterView : public juce::Component
{
public:
    explicit MeterView (MilagrosMasterProcessor& p) : proc (p) {}

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (panelColour);
        g.fillRoundedRectangle (r, 8.0f);

        const auto m = proc.getMeters();
        const int tIdx = (int) proc.apvts.getRawParameterValue ("target")->load();
        const float target = MilagrosMasterProcessor::targetLufs (tIdx);
        const float ceilingDb = proc.apvts.getRawParameterValue ("ceiling")->load();

        auto area = getLocalBounds().reduced (12, 8);
        auto top = area.removeFromTop (50);
        const int colW = top.getWidth() / 3;
        drawBig (g, top.removeFromLeft (colW), es ("Volumen del tema"), fmt (m.integrated, "LUFS"), cyan);
        drawBig (g, top.removeFromLeft (colW), es ("Volumen ahora (3 s)"), fmt (m.shortTerm, "LUFS"), juce::Colours::white);
        drawBig (g, top, es ("Pico máximo"), fmt (m.outPeakDb, "dB"),
                 m.outPeakDb > ceilingDb + 0.2f ? juce::Colours::red : juce::Colours::white);

        // Barra de -24 a -4 LUFS con el objetivo marcado
        area.removeFromTop (6);
        auto bar = area.removeFromTop (12).toFloat();
        auto xOf = [&] (float lufs) { return bar.getX() + (juce::jlimit (-24.0f, -4.0f, lufs) + 24.0f) / 20.0f * bar.getWidth(); };
        g.setColour (juce::Colour (0xff2e3342));
        g.fillRoundedRectangle (bar, 4.0f);
        if (m.integrated > -100.0f)
        {
            g.setColour (cyan);
            g.fillRoundedRectangle (bar.withRight (xOf (m.integrated)), 4.0f);
        }
        g.setColour (accent);
        g.fillRect (xOf (target) - 1.5f, bar.getY() - 3.0f, 3.0f, bar.getHeight() + 6.0f);

        g.setFont (11.0f);
        g.setColour (juce::Colours::grey);
        auto labels = area.removeFromTop (16);
        g.drawText ("-24", labels, juce::Justification::centredLeft);
        g.drawText ("-4 LUFS", labels, juce::Justification::centredRight);
        g.setColour (accent);
        g.drawText (es ("Objetivo: ") + juce::String (target, 0) + " LUFS (barra naranja)", labels, juce::Justification::centred);

        // Datos extra
        area.removeFromTop (4);
        g.setColour (juce::Colours::lightgrey);
        g.setFont (13.0f);
        g.drawText (es ("Limitador: ") + juce::String (m.limGrDb, 1) + es (" dB ahora   ·   Fase estéreo: ")
                        + juce::String (m.corr, 2) + es ("   (1 = perfecta, menos de 0,2 = revisar)"),
                    area.removeFromTop (18), juce::Justification::centredLeft);

        if (proc.apvts.getRawParameterValue ("compare")->load() > 0.5f)
        {
            g.setColour (accent);
            g.drawText (es ("ESCUCHANDO EL ORIGINAL (con el mismo volumen que el master)"),
                        area.removeFromTop (18), juce::Justification::centredLeft);
        }
    }

private:
    static void drawBig (juce::Graphics& g, juce::Rectangle<int> r, const juce::String& label,
                         const juce::String& value, juce::Colour c)
    {
        g.setColour (juce::Colours::grey);
        g.setFont (11.0f);
        g.drawText (label, r.removeFromTop (14), juce::Justification::centredLeft);
        g.setColour (c);
        g.setFont (22.0f);
        g.drawText (value, r, juce::Justification::centredLeft);
    }

    MilagrosMasterProcessor& proc;
};

//==============================================================================
// PÁGINA 1: master
class MilagrosMasterEditor::MainPage : public juce::Component
{
public:
    explicit MainPage (MilagrosMasterProcessor& p) : proc (p), meters (p)
    {
        presetBox.addItem (es ("Estilo (elige tu género)"), 1);
        for (int i = 0; i < (int) std::size (presets); ++i)
            presetBox.addItem (es (presets[i].name), i + 2);
        presetBox.setSelectedId (1, juce::dontSendNotification);
        presetBox.onChange = [this] { applyPreset (presetBox.getSelectedId() - 2); };
        addAndMakeVisible (presetBox);

        if (auto* prm = dynamic_cast<juce::AudioParameterChoice*> (proc.apvts.getParameter ("target")))
            targetBox.addItemList (prm->choices, 1);
        addAndMakeVisible (targetBox);
        targetAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, "target", targetBox);

        bassMonoBtn.setButtonText (es ("Graves en mono"));
        compareBtn.setButtonText (es ("Comparar con el original (mismo volumen)"));
        for (auto* b : { &bassMonoBtn, &compareBtn })
        {
            b->setColour (juce::ToggleButton::textColourId, juce::Colours::white);
            b->setColour (juce::ToggleButton::tickColourId, accent);
            addAndMakeVisible (*b);
        }
        bassMonoAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, "bassMono", bassMonoBtn);
        compareAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, "compare", compareBtn);

        addAndMakeVisible (meters);

        setupKnob (trim,    "trim",    es ("Entrada"));
        setupKnob (lows,    "lows",    es ("Graves"));
        setupKnob (bright,  "bright",  es ("Brillo"));
        setupKnob (glue,    "glue",    es ("Pegamento"));
        setupKnob (width,   "width",   es ("Ancho"));
        setupKnob (warm,    "warm",    es ("Calidez"));
        setupKnob (drive,   "drive",   es ("Intensidad"));
        setupKnob (ceiling, "ceiling", es ("Techo"));
        drive.slider.setColour (juce::Slider::rotarySliderFillColourId, accent);
        drive.label.setColour (juce::Label::textColourId, accent);
    }

    void refresh() { meters.repaint(); }

    void paint (juce::Graphics& g) override
    {
        g.setColour (juce::Colours::grey);
        g.setFont (12.0f);
        g.drawText (es ("MilagrosMaster v0.1 · gratis y de código abierto"),
                    getLocalBounds().removeFromBottom (20), juce::Justification::centred);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (12);
        r.removeFromBottom (20);

        auto top = r.removeFromTop (30);
        presetBox.setBounds (top.removeFromLeft (230));
        top.removeFromLeft (8);
        targetBox.setBounds (top);

        r.removeFromTop (8);
        meters.setBounds (r.removeFromTop (160));
        r.removeFromTop (4);

        auto toggles = r.removeFromTop (28);
        bassMonoBtn.setBounds (toggles.removeFromLeft (150));
        compareBtn.setBounds (toggles);
        r.removeFromTop (4);

        auto row1 = r.removeFromTop (r.getHeight() / 2);
        layoutRow (row1, { &trim, &lows, &bright, &glue });
        layoutRow (r, { &width, &warm, &drive, &ceiling });
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

    static void layoutRow (juce::Rectangle<int> area, std::vector<Knob*> knobs)
    {
        const int w = area.getWidth() / (int) knobs.size();
        for (auto* k : knobs)
        {
            auto cell = area.removeFromLeft (w).reduced (4);
            k->label.setBounds (cell.removeFromTop (20));
            k->slider.setBounds (cell);
        }
    }

    void applyPreset (int idx)
    {
        if (idx < 0 || idx >= (int) std::size (presets))
            return;
        const auto& p = presets[idx];
        setFloat (proc.apvts, "lows", p.lows);
        setFloat (proc.apvts, "bright", p.bright);
        setFloat (proc.apvts, "glue", p.glue);
        setFloat (proc.apvts, "width", p.width);
        setFloat (proc.apvts, "warm", p.warm);
        setFloat (proc.apvts, "drive", p.drive);
        setChoice (proc.apvts, "target", p.target);
        proc.resetMeters();
    }

    MilagrosMasterProcessor& proc;
    MeterView meters;
    juce::ComboBox presetBox, targetBox;
    juce::ToggleButton bassMonoBtn, compareBtn;
    Knob trim, lows, bright, glue, width, warm, drive, ceiling;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> targetAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bassMonoAtt, compareAtt;
};

//==============================================================================
// PÁGINA 2: hoja de consejos
namespace
{
    struct Advice
    {
        juce::String text;
        bool canAuto = false;
        float newDrive = 0.0f;
    };

    Advice buildAdvice (const MilagrosMasterProcessor::Meters& m, float target, float drive)
    {
        Advice r;

        if (m.validBlocks < 30)
        {
            r.text = es ("Reproduce tu tema completo (de principio a fin) con MilagrosMaster en el canal master.\n\n"
                         "Los consejos aparecen cuando haya al menos unos 3 segundos de música medida.\n\n"
                         "ANTES DE MASTERIZAR: deja tu mezcla con picos de unos -6 dB (no pegada al tope). "
                         "Si llega muy fuerte, el master no puede arreglarla.");
            return r;
        }

        juce::StringArray lines;

        // 1) Volumen
        const float diff = target - m.integrated;
        if (std::abs (diff) <= 0.7f)
        {
            lines.add (es ("• VOLUMEN: tu tema está en ") + juce::String (m.integrated, 1) + es (" LUFS y el objetivo es ")
                       + juce::String (target, 0) + es (". ¡Justo en el objetivo!"));
        }
        else
        {
            r.canAuto = true;
            r.newDrive = juce::jlimit (0.0f, 18.0f, drive + diff);
            lines.add (es ("• VOLUMEN: tu tema está en ") + juce::String (m.integrated, 1) + es (" LUFS y el objetivo es ")
                       + juce::String (target, 0) + (diff > 0 ? es (": le falta volumen. Sube Intensidad unos ")
                                                              : es (": se pasa de volumen. Baja Intensidad unos "))
                       + juce::String (std::abs (diff), 1) + es (" dB (o pulsa el botón automático y vuelve a reproducir el tema; "
                                                                 "puede hacer falta repetirlo una vez más)."));
            if (diff > 0 && drive >= 17.9f)
                lines.add (es ("   Ya estás al máximo de Intensidad: mejor sube el volumen en la mezcla o revisa que los graves no estén tapando todo."));
        }
        if (target >= -9.0f)
            lines.add (es ("• Nota: el volumen fuerte (-9 / -8 LUFS) es un estilo, no una regla. Plataformas como Spotify o YouTube "
                           "suelen bajar los temas muy fuertes, así que en streaming no ganas volumen real y pierdes pegada. "
                           "Úsalo si quieres el sonido de club."));

        // 2) Margen de entrada
        if (m.inPeakDb > -3.0f)
            lines.add (es ("• ENTRADA: tus picos llegan a ") + juce::String (m.inPeakDb, 1)
                       + es (" dB. Antes de masterizar baja el volumen de tu mezcla a unos -6 dB (o usa la perilla Entrada). "
                             "Una mezcla pegada al tope deja al master sin margen."));

        // 3) Limitador
        if (m.heavyFrac > 0.25f)
            lines.add (es ("• LIMITADOR: trabaja con más de 3 dB durante el ") + juce::String (juce::roundToInt (m.heavyFrac * 100.0f))
                       + es (" % del tema. Baja Intensidad o revisa la mezcla: un 808 o un bombo muy fuertes hacen que el limitador "
                             "aplaste todo lo demás."));
        else if (m.limGrMaxDb < -8.0f)
            lines.add (es ("• LIMITADOR: en algún momento baja ") + juce::String (-m.limGrMaxDb, 1)
                       + es (" dB de golpe. Probablemente hay un golpe (bombo/808) mucho más fuerte que el resto."));

        // 4) Pegada (diferencia entre pico y volumen)
        const float plr = m.outPeakDb - m.integrated;
        if (plr < 6.0f)
            lines.add (es ("• PEGADA: tu tema está muy aplastado (poca diferencia entre picos y volumen). Baja Intensidad o Pegamento."));
        else if (plr > 15.0f)
            lines.add (es ("• PEGADA: tu tema es muy dinámico; puede sonar bajo al lado de otros. Sube Pegamento o Intensidad."));

        // 5) Fase / mono
        if (m.corr < 0.0f)
            lines.add (es ("• FASE: problema serio. Parte de tu tema se puede cancelar en mono (celular, parlante). Baja el Ancho o revisa sintetizadores y efectos estéreo."));
        else if (m.corr < 0.2f)
            lines.add (es ("• FASE: tu tema es muy ancho y puede perder fuerza en mono. Baja el Ancho."));
        if (m.sideMidDb > -3.0f)
            lines.add (es ("• ANCHO: tienes muchísimo sonido en los lados. Baja Ancho para que la voz y el bombo no se debiliten."));

        // 6) Graves
        if (m.lowRatio > 0.8f)
            lines.add (es ("• GRAVES: casi toda la energía está en los graves. Puede ahogar la voz y sonar mal en celulares. Baja Graves o revisa el 808."));
        else if (m.lowRatio < 0.08f)
            lines.add (es ("• GRAVES: casi no hay graves. En trap, reggaeton o drill suele hacer falta más: sube Graves o revisa tu 808/bajo."));

        lines.add (es ("\nAL EXPORTAR: WAV de 24 bits, sin normalizar, con picos máximos de -1 dB. "
                       "Sube a plataformas desde ese WAV (no desde un MP3)."));
        lines.add (es ("COMPARAR JUSTO: usa el botón \"Comparar con el original\": el plugin iguala el volumen, "
                       "porque lo más fuerte siempre parece mejor aunque no lo sea."));
        lines.add (es ("\nGLOSARIO\nLUFS: la unidad de volumen que usan las plataformas.\n"
                       "Pico: el punto más alto de la onda; nunca debe pasar del Techo.\n"
                       "Limitador: evita que el sonido se pase del tope.\n"
                       "Ancho: qué tan abierto se escucha el estéreo. Mono: izquierda y derecha iguales (un celular)."));

        r.text = lines.joinIntoString ("\n\n");
        return r;
    }
}

class MilagrosMasterEditor::AdvisorPage : public juce::Component
{
public:
    explicit AdvisorPage (MilagrosMasterProcessor& p) : proc (p)
    {
        text.setMultiLine (true);
        text.setReadOnly (true);
        text.setCaretVisible (false);
        text.setColour (juce::TextEditor::backgroundColourId, panelColour);
        text.setColour (juce::TextEditor::textColourId, juce::Colours::white);
        text.setColour (juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
        addAndMakeVisible (text);

        autoBtn.setButtonText (es ("Ajustar volumen automáticamente"));
        resetBtn.setButtonText (es ("Reiniciar medidores"));
        addAndMakeVisible (autoBtn);
        addAndMakeVisible (resetBtn);

        autoBtn.onClick = [this]
        {
            if (advice.canAuto)
            {
                setFloat (proc.apvts, "drive", advice.newDrive);
                proc.resetMeters();
            }
        };
        resetBtn.onClick = [this] { proc.resetMeters(); };
        refresh();
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (12);
        auto row = r.removeFromBottom (34);
        r.removeFromBottom (8);
        autoBtn.setBounds (row.removeFromLeft (row.getWidth() / 2 - 4));
        row.removeFromLeft (8);
        resetBtn.setBounds (row);
        text.setBounds (r);
    }

    void refresh()
    {
        const auto m = proc.getMeters();
        const float target = MilagrosMasterProcessor::targetLufs ((int) proc.apvts.getRawParameterValue ("target")->load());
        const float drive = proc.apvts.getRawParameterValue ("drive")->load();
        advice = buildAdvice (m, target, drive);
        if (text.getText() != advice.text)
            text.setText (advice.text, false);
        autoBtn.setEnabled (advice.canAuto);
    }

private:
    MilagrosMasterProcessor& proc;
    juce::TextEditor text;
    juce::TextButton autoBtn, resetBtn;
    Advice advice;
};

//==============================================================================
MilagrosMasterEditor::MilagrosMasterEditor (MilagrosMasterProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    mainPage = std::make_unique<MainPage> (proc);
    advisorPage = std::make_unique<AdvisorPage> (proc);

    tabs.setTabBarDepth (32);
    tabs.setOutline (0);
    tabs.addTab (es ("Master"), panelColour, mainPage.get(), false);
    tabs.addTab (es ("Asesor (hoja de consejos)"), panelColour, advisorPage.get(), false);
    addAndMakeVisible (tabs);

    setSize (760, 580);
    startTimerHz (20);
}

MilagrosMasterEditor::~MilagrosMasterEditor()
{
    stopTimer();
}

void MilagrosMasterEditor::paint (juce::Graphics& g)
{
    g.fillAll (bgColour);
}

void MilagrosMasterEditor::resized()
{
    tabs.setBounds (getLocalBounds());
}

void MilagrosMasterEditor::timerCallback()
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
