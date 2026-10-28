// StemFetch.h  -- JUCE 7/8 VST3 (instrument, 4 stereo outputs)
// In Projucer / CMake: plugin is a synth (IsSynth), 4 stereo output buses,
// modules: juce_audio_basics, juce_audio_formats, juce_audio_processors,
//          juce_audio_utils, juce_gui_basics, juce_core
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_gui_basics/juce_gui_basics.h>

static const juce::StringArray kStemNames { "vocals", "drums", "bass", "other" };

//==============================================================================
class StemFetchProcessor : public juce::AudioProcessor,
                           private juce::Thread
{
public:
    StemFetchProcessor()
        : AudioProcessor(BusesProperties()
              .withOutput("Vocals", juce::AudioChannelSet::stereo(), true)
              .withOutput("Drums",  juce::AudioChannelSet::stereo(), true)
              .withOutput("Bass",   juce::AudioChannelSet::stereo(), true)
              .withOutput("Other",  juce::AudioChannelSet::stereo(), true)),
          juce::Thread("StemFetch worker")
    {
        formats.registerBasicFormats();
        cacheRoot = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                        .getChildFile("StemFetch");
    }
    ~StemFetchProcessor() override { stopThread(2000); }

    // ---- job control (message thread) ----
    void startJob(const juce::String& u, const juce::File& python, const juce::File& script)
    {
        if (isThreadRunning()) return;
        url = u; pythonExe = python; backendScript = script;
        setStatus("Starting...", 0.0f);
        startThread();
    }
    juce::String getStatus() const { const juce::ScopedLock l(statusLock); return status; }
    float getProgress() const { return progress.load(); }
    juce::File getStemFile(const juce::String& n) const
    { const juce::ScopedLock l(statusLock); return stemFiles[n]; }

    //==============================================================================
    void prepareToPlay(double sr, int) override { hostRate = sr; }
    void releaseResources() override {}

    bool isBusesLayoutSupported(const BusesLayout& l) const override
    {
        for (auto& o : l.outputBuses)
            if (o != juce::AudioChannelSet::stereo() && !o.isDisabled()) return false;
        return true;
    }

    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override
    {
        buffer.clear();
        juce::int64 pos = 0; bool playing = false;
        if (auto* ph = getPlayHead())
            if (auto p = ph->getPosition()) {
                playing = p->getIsPlaying();
                pos = p->getTimeInSamples().orFallback(0);
            }
        if (!playing) return;

        const juce::SpinLock::ScopedTryLockType lock(stemLock);
        if (!lock.isLocked()) return;

        for (int s = 0; s < stems.size() && s < getBusCount(false); ++s) {
            auto out = getBusBuffer(buffer, false, s);
            auto& src = stems.getReference(s);
            if (src.getNumSamples() == 0) continue;
            for (int i = 0; i < out.getNumSamples(); ++i) {
                auto idx = pos + i;
                if (idx < 0 || idx >= src.getNumSamples()) continue;
                for (int ch = 0; ch < out.getNumChannels(); ++ch)
                    out.setSample(ch, i, src.getSample(juce::jmin(ch, src.getNumChannels() - 1), (int) idx));
            }
        }
    }

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "StemFetch"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override {}
    void setStateInformation(const void*, int) override {}

private:
    void setStatus(const juce::String& s, float p)
    { { const juce::ScopedLock l(statusLock); status = s; } progress = p; }

    // ---- worker thread: run backend, parse JSON lines ----
    void run() override
    {
        auto outDir = cacheRoot.getChildFile(juce::String(url.hashCode64()));
        juce::ChildProcess proc;
        juce::StringArray args { pythonExe.getFullPathName(), backendScript.getFullPathName(),
                                 url, outDir.getFullPathName() };
        if (!proc.start(args)) { setStatus("Could not start Python", 0); return; }

        juce::String buf;
        while (proc.isRunning() && !threadShouldExit()) {
            char tmp[1024];
            auto n = proc.readProcessOutput(tmp, sizeof(tmp) - 1);
            if (n > 0) { tmp[n] = 0; buf += tmp; }
            while (buf.contains("\n")) {
                handleLine(buf.upToFirstOccurrenceOf("\n", false, false));
                buf = buf.fromFirstOccurrenceOf("\n", false, false);
            }
            wait(100);
        }
        buf += proc.readAllProcessOutput();
        for (auto& line : juce::StringArray::fromLines(buf)) handleLine(line);
    }

    void handleLine(const juce::String& line)
    {
        auto v = juce::JSON::parse(line.trim());
        if (!v.isObject()) return;
        auto st = v["status"].toString();
        if (st == "error")       setStatus("Error: " + v["message"].toString(), 0);
        else if (st == "done")   { loadStems(v["stems"]); setStatus("Ready", 1.0f); }
        else                     setStatus(st + "...", (float)(double) v["progress"]);
    }

    void loadStems(const juce::var& map)
    {
        juce::Array<juce::AudioBuffer<float>> loaded;
        juce::HashMap<juce::String, juce::File> files;
        for (auto& name : kStemNames) {
            juce::File f(map[name.toStdString().c_str()].toString());
            files.set(name, f);
            juce::AudioBuffer<float> b;
            if (std::unique_ptr<juce::AudioFormatReader> r { formats.createReaderFor(f) }) {
                juce::AudioBuffer<float> raw((int) r->numChannels, (int) r->lengthInSamples);
                r->read(&raw, 0, (int) r->lengthInSamples, 0, true, true);
                double ratio = r->sampleRate / juce::jmax(1.0, hostRate);
                int outLen = (int) (raw.getNumSamples() / ratio);
                b.setSize(raw.getNumChannels(), outLen);
                for (int ch = 0; ch < raw.getNumChannels(); ++ch) {
                    juce::LagrangeInterpolator li;
                    li.process(ratio, raw.getReadPointer(ch), b.getWritePointer(ch), outLen);
                }
            }
            loaded.add(std::move(b));
        }
        { const juce::SpinLock::ScopedLockType l(stemLock); stems = std::move(loaded); }
        { const juce::ScopedLock l(statusLock);
          for (auto& n : kStemNames) stemFiles.set(n, files[n]); }
    }

    juce::AudioFormatManager formats;
    juce::Array<juce::AudioBuffer<float>> stems;
    juce::SpinLock stemLock;
    mutable juce::CriticalSection statusLock;
    juce::String status { "Idle" }, url;
    juce::HashMap<juce::String, juce::File> stemFiles;
    std::atomic<float> progress { 0.0f };
    juce::File cacheRoot, pythonExe, backendScript;
    double hostRate = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StemFetchProcessor)
};

//==============================================================================
// Draggable tile: drag it into FL's Playlist / Channel Rack to drop the stem as audio.
struct StemTile : public juce::Component
{
    StemTile(StemFetchProcessor& p, juce::String n) : proc(p), name(std::move(n)) {}
    void paint(juce::Graphics& g) override
    {
        g.setColour(juce::Colour(0xff2b2f3a)); g.fillRoundedRectangle(getLocalBounds().toFloat(), 6.f);
        g.setColour(juce::Colours::white); g.drawText(name, getLocalBounds(), juce::Justification::centred);
    }
    void mouseDrag(const juce::MouseEvent& e) override
    {
        if (e.getDistanceFromDragStart() < 6) return;
        auto f = proc.getStemFile(name);
        if (f.existsAsFile())
            juce::DragAndDropContainer::performExternalDragDropOfFiles({ f.getFullPathName() }, false);
    }
    StemFetchProcessor& proc; juce::String name;
};

class StemFetchEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit StemFetchEditor(StemFetchProcessor& p) : AudioProcessorEditor(p), proc(p)
    {
        url.setTextToShowWhenEmpty("Paste YouTube link", juce::Colours::grey);
        fetch.setButtonText("Fetch + Split");
        fetch.onClick = [this] {
            // TODO: make these paths configurable (settings file / file chooser)
            proc.startJob(url.getText(),
                          juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                              .getChildFile("StemFetch/venv/Scripts/python.exe"),
                          juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                              .getChildFile("StemFetch/fetch_and_split.py"));
        };
        addAndMakeVisible(url); addAndMakeVisible(fetch); addAndMakeVisible(statusLabel);
        for (auto& n : kStemNames) { tiles.add(new StemTile(p, n)); addAndMakeVisible(tiles.getLast()); }
        setSize(480, 220); startTimerHz(5);
    }
    void resized() override
    {
        auto a = getLocalBounds().reduced(10);
        auto top = a.removeFromTop(30);
        fetch.setBounds(top.removeFromRight(110)); url.setBounds(top.reduced(0, 0).withTrimmedRight(6));
        statusLabel.setBounds(a.removeFromTop(30));
        auto row = a.removeFromTop(80); int w = row.getWidth() / 4;
        for (auto* t : tiles) t->setBounds(row.removeFromLeft(w).reduced(4));
    }
    void paint(juce::Graphics& g) override { g.fillAll(juce::Colour(0xff1a1d24)); }
private:
    void timerCallback() override
    { statusLabel.setText(proc.getStatus() + "  (" + juce::String((int)(proc.getProgress() * 100)) + "%)",
                          juce::dontSendNotification); }
    StemFetchProcessor& proc;
    juce::TextEditor url; juce::TextButton fetch; juce::Label statusLabel;
    juce::OwnedArray<StemTile> tiles;
};

inline juce::AudioProcessorEditor* StemFetchProcessor::createEditor() { return new StemFetchEditor(*this); }

// In PluginMain.cpp:
// juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new StemFetchProcessor(); }
