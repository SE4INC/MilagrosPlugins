#include "PluginEditor.h"

namespace
{
    const juce::Colour bgColour    (0xff14161c);
    const juce::Colour panelColour (0xff1d2029);
    const juce::Colour accent      (0xffffb347);
}

//==============================================================================
// PÁGINA 1: perillas
class MilagrosSoundEditor::MainPage : public juce::Component
{
public:
    explicit MainPage (juce::AudioProcessorValueTreeState& s)
    {
        setupCombo (voiceBox, voiceAtt, s, "voice");
        setupCombo (micBox, micAtt, s, "mic");

        setupKnob (enhance, s, "enhance", es ("Mejorar voz"));
        setupKnob (clarity, s, "clarity", es ("Claridad"));
        setupKnob (warmth,  s, "warmth",  es ("Calidez"));
        setupKnob (air,     s, "air",     es ("Aire"));
        setupKnob (mud,     s, "mud",     es ("Anti-barro"));
        setupKnob (deess,   s, "deess",   es ("De-esser"));
        setupKnob (comp,    s, "comp",    es ("Compresión"));
        setupKnob (mix,     s, "mix",     es ("Mezcla"));
        setupKnob (output,  s, "output",  es ("Salida"));

        enhance.slider.setColour (juce::Slider::rotarySliderFillColourId, accent);
        enhance.label.setColour (juce::Label::textColourId, accent);
    }

    void paint (juce::Graphics& g) override
    {
        g.setColour (juce::Colours::grey);
        g.setFont (13.0f);
        g.drawText (es ("MilagrosSound v0.1 · gratis y de código abierto"),
                    getLocalBounds().removeFromBottom (22), juce::Justification::centred);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (12);
        r.removeFromBottom (22);

        auto top = r.removeFromTop (30);
        voiceBox.setBounds (top.removeFromLeft (top.getWidth() / 2 - 4));
        top.removeFromLeft (8);
        micBox.setBounds (top);

        r.removeFromTop (8);
        auto row1 = r.removeFromTop (r.getHeight() / 2);
        layoutRow (row1, { &enhance, &clarity, &warmth, &air, &mud });
        layoutRow (r,    { &deess, &comp, &mix, &output });
    }

private:
    struct Knob
    {
        juce::Slider slider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    void setupKnob (Knob& k, juce::AudioProcessorValueTreeState& s, const char* id, const juce::String& text)
    {
        k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 18);
        k.slider.setColour (juce::Slider::rotarySliderFillColourId, juce::Colour (0xff7ad7f0));
        k.label.setText (text, juce::dontSendNotification);
        k.label.setJustificationType (juce::Justification::centred);
        k.label.setColour (juce::Label::textColourId, juce::Colours::white);
        addAndMakeVisible (k.slider);
        addAndMakeVisible (k.label);
        k.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (s, id, k.slider);
    }

    void setupCombo (juce::ComboBox& box,
                     std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>& att,
                     juce::AudioProcessorValueTreeState& s, const char* id)
    {
        if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (s.getParameter (id)))
            box.addItemList (p->choices, 1);
        addAndMakeVisible (box);
        att = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (s, id, box);
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

    juce::ComboBox voiceBox, micBox;
    Knob enhance, clarity, warmth, air, mud, deess, comp, mix, output;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> voiceAtt, micAtt;
};

//==============================================================================
// PÁGINA 2: el Asesor
namespace
{
    struct Advice
    {
        juce::String text;
        float dMud = 0.0f, dClarity = 0.0f, dAir = 0.0f, dDeess = 0.0f;
        bool hasChanges() const { return dMud + dClarity + dAir + dDeess > 0.0f; }
    };

    Advice buildAdvice (const MilagrosSoundProcessor::Analysis& a)
    {
        Advice r;

        if (a.frames < 25)
        {
            r.text = es ("Habla o canta unos segundos cerca del micrófono, con MilagrosSound puesto en la pista de tu voz.\n\n"
                         "Aquí aparecerán consejos personalizados según cómo suena tu micrófono y tu voz.");
            return r;
        }

        // Valores relativos a la zona media (500 Hz - 2 kHz)
        const float rLow  = a.lowDb - a.midDb;
        const float rMud  = a.mudDb - a.midDb;
        const float rPres = a.presenceDb - a.midDb;
        const float rSib  = a.sibDb - a.midDb;
        const float rAir  = a.airDb - a.midDb;

        juce::StringArray tips;

        if (a.peakDb > -1.0f)
            tips.add (es ("• Tu señal llega al tope (picos cerca de 0 dB). Baja la ganancia del micrófono o de la tarjeta de audio para evitar distorsión."));
        if (a.rmsDb < -32.0f)
            tips.add (es ("• Tu nivel es muy bajo. Acércate a unos 10-15 cm del micrófono o sube un poco la ganancia de entrada."));
        if (rLow > -8.0f)
            tips.add (es ("• Hay mucho ruido grave (soplidos, golpes, ruido del cuarto). Usa filtro anti-pop, no hables directo al centro del micrófono y deja el corte de graves activo."));
        if (rMud > 10.0f)
        {
            tips.add (es ("• Tu voz suena 'encajonada' (zona de 200-400 Hz alta). Sube Anti-barro. Probablemente el cuarto o la cercanía al micrófono lo causan."));
            r.dMud = 0.2f;
        }
        if (rPres < -18.0f)
        {
            tips.add (es ("• Falta claridad: la voz se oye opaca. Sube Claridad para que la voz quede al frente de la mezcla."));
            r.dClarity = 0.2f;
        }
        if (rSib > -12.0f)
        {
            tips.add (es ("• Las 'eses' suenan duras. Sube De-esser y prueba cantar un poco de lado respecto al micrófono."));
            r.dDeess = 0.25f;
        }
        if (rAir < -45.0f)
        {
            tips.add (es ("• Falta aire y brillo arriba. Sube Aire, pero con cuidado: también puede subir el ruido de fondo."));
            r.dAir = 0.2f;
        }

        if (tips.isEmpty())
            tips.add (es ("• Tu voz se ve bastante equilibrada. Ajusta Calidez y Compresión a gusto y confía en tus oídos."));

        r.text = es ("Pico: ") + juce::String (a.peakDb, 1) + " dB   |   "
               + es ("Nivel: ") + juce::String (a.rmsDb, 1) + " dB\n\n"
               + tips.joinIntoString ("\n\n")
               + es ("\n\nEstas sugerencias son orientativas (se basan en el promedio de tu señal). "
                     "Los umbrales se pueden mejorar con ayuda de la comunidad.");
        return r;
    }
}

class MilagrosSoundEditor::AdvisorPage : public juce::Component
{
public:
    AdvisorPage (MilagrosSoundProcessor& p) : proc (p)
    {
        text.setMultiLine (true);
        text.setReadOnly (true);
        text.setCaretVisible (false);
        text.setColour (juce::TextEditor::backgroundColourId, panelColour);
        text.setColour (juce::TextEditor::textColourId, juce::Colours::white);
        text.setColour (juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
        addAndMakeVisible (text);

        applyButton.setButtonText (es ("Aplicar sugerencias"));
        resetButton.setButtonText (es ("Reiniciar análisis"));
        addAndMakeVisible (applyButton);
        addAndMakeVisible (resetButton);

        applyButton.onClick = [this] { applySuggestions(); };
        resetButton.onClick = [this] { proc.resetAnalysis(); refresh(); };

        refresh();
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (12);
        auto buttons = r.removeFromBottom (34);
        r.removeFromBottom (8);
        applyButton.setBounds (buttons.removeFromLeft (buttons.getWidth() / 2 - 4));
        buttons.removeFromLeft (8);
        resetButton.setBounds (buttons);
        text.setBounds (r);
    }

    void refresh()
    {
        lastAdvice = buildAdvice (proc.getAnalysis());
        if (text.getText() != lastAdvice.text)
            text.setText (lastAdvice.text, false);
        applyButton.setEnabled (lastAdvice.hasChanges());
    }

private:
    void applySuggestions()
    {
        auto bump = [this] (const char* id, float delta)
        {
            if (delta <= 0.0f)
                return;
            if (auto* prm = proc.apvts.getParameter (id))
            {
                prm->beginChangeGesture();
                prm->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, prm->getValue() + delta));
                prm->endChangeGesture();
            }
        };
        bump ("mud", lastAdvice.dMud);
        bump ("clarity", lastAdvice.dClarity);
        bump ("air", lastAdvice.dAir);
        bump ("deess", lastAdvice.dDeess);
    }

    MilagrosSoundProcessor& proc;
    juce::TextEditor text;
    juce::TextButton applyButton, resetButton;
    Advice lastAdvice;
};

//==============================================================================
MilagrosSoundEditor::MilagrosSoundEditor (MilagrosSoundProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    mainPage = std::make_unique<MainPage> (proc.apvts);
    advisorPage = std::make_unique<AdvisorPage> (proc);

    tabs.setTabBarDepth (32);
    tabs.setOutline (0);
    tabs.addTab (es ("Mejorar voz"), panelColour, mainPage.get(), false);
    tabs.addTab (es ("Asesor"), panelColour, advisorPage.get(), false);
    addAndMakeVisible (tabs);

    setSize (620, 440);
    startTimer (300);
}

MilagrosSoundEditor::~MilagrosSoundEditor()
{
    stopTimer();
}

void MilagrosSoundEditor::paint (juce::Graphics& g)
{
    g.fillAll (bgColour);
}

void MilagrosSoundEditor::resized()
{
    tabs.setBounds (getLocalBounds());
}

void MilagrosSoundEditor::timerCallback()
{
    if (tabs.getCurrentTabIndex() == 1)
        advisorPage->refresh();
}
