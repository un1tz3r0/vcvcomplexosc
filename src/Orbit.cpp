#include <atomic>
#include "plugin.hpp"
#include "OledDisplay.hpp"
#include "PanelArt.hpp"
#include "core/OrbitScene.hpp"

// Sweeps an elliptical ring through a 3D field and plays what it reads.
struct Orbit : Module {
	static constexpr int MODS = 12, OUTS = 4;

	// The first MODS params each have a CV input and an attenuverter at the same offset.
	enum ParamId {
		FREQ_PARAM, X_PARAM, Y_PARAM, Z_PARAM, MAJOR_PARAM, MINOR_PARAM,
		DRIFT_PARAM, SPIN_PARAM, TILT_PARAM, AZIMUTH_PARAM, SPREAD_PARAM, ANGLE_PARAM,
		ENUMS(ATTEN_PARAM, MODS),
		FIELD_PARAM, OCTAVES_PARAM, MODE_PARAM, RANGE_PARAM,
		PARAMS_LEN
	};
	enum InputId { ENUMS(CV_INPUT, MODS), VOCT_INPUT, SYNC_INPUT, INPUTS_LEN };
	enum OutputId { ENUMS(WAVE_OUTPUT, OUTS), X_OUTPUT, Y_OUTPUT, Z_OUTPUT, OUTPUTS_LEN };

	struct FreqQuantity : ParamQuantity {
		float getDisplayValue() override { return float(static_cast<Orbit*>(module)->baseHz() * std::exp2(getValue())); }
		void setDisplayValue(float hz) override { setValue(std::log2(hz / static_cast<Orbit*>(module)->baseHz())); }
	};

	struct Voice : cxo::OrbitVoice {
		dsp::SchmittTrigger sync;
	};

	Voice voices[PORT_MAX_CHANNELS];
	float perVolt[MODS];
	int64_t seed = 0;
	bool center = true, normalize = true, autoRotate = true, showStrip = true;

	// Channel 0's state for the display, double-buffered so the UI thread never reads a half-written one.
	cxo::OrbitState states[2];
	std::atomic<int> front{0};
	dsp::ClockDivider publishDivider;

	Orbit() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, 0);
		configParam<FreqQuantity>(FREQ_PARAM, -4.f, 4.f, 0.f, "Frequency", " Hz");
		configParam(X_PARAM, -5.f, 5.f, 0.f, "Center X");
		configParam(Y_PARAM, -5.f, 5.f, 0.f, "Center Y");
		configParam(Z_PARAM, -5.f, 5.f, 0.f, "Center Z");
		configParam(MAJOR_PARAM, 0.f, 4.f, 1.2f, "Major radius");
		configParam(MINOR_PARAM, 0.f, 4.f, 0.8f, "Minor radius");
		configParam(DRIFT_PARAM, -1.f, 1.f, 0.f, "Drift along Z", " units/s");
		configParam(SPIN_PARAM, 0.f, 360.f, 0.f, "Spin", "°");
		configParam(TILT_PARAM, -90.f, 90.f, 0.f, "Tilt", "°");
		configParam(AZIMUTH_PARAM, 0.f, 360.f, 0.f, "Azimuth", "°");
		configParam(SPREAD_PARAM, 0.f, 1.f, 1.f, "Spread", "%", 0.f, 100.f);
		configParam(ANGLE_PARAM, 0.f, 360.f, 0.f, "Angle", "°");
		for (int i = 0; i < MODS; ++i) {
			const std::string name = paramQuantities[i]->name;
			configParam(ATTEN_PARAM + i, -1.f, 1.f, 0.f, name + " CV", "%", 0.f, 100.f);
			configInput(CV_INPUT + i, i == FREQ_PARAM ? "Exponential FM" : name + " CV");
			perVolt[i] = i == FREQ_PARAM ? 1.f : (paramQuantities[i]->maxValue - paramQuantities[i]->minValue) / 10.f;
		}
		configSwitch(FIELD_PARAM, 0.f, 2.f, 0.f, "Field", {"Simplex 3D", "Simplex 2D", "Gyroid"});
		configParam(OCTAVES_PARAM, 1.f, 4.f, 1.f, "Octaves")->snapEnabled = true;
		configSwitch(MODE_PARAM, 0.f, 1.f, 0.f, "Output mode", {"Phase: outputs spread around one ring", "Pinch: rings that meet at the angle"});
		configSwitch(RANGE_PARAM, 0.f, 1.f, 1.f, "Range", {"LFO", "Audio"});
		configInput(VOCT_INPUT, "1V/octave pitch");
		configInput(SYNC_INPUT, "Hard sync");
		for (int k = 0; k < OUTS; ++k)
			configOutput(WAVE_OUTPUT + k, string::f("Wave %d", k + 1));
		configOutput(X_OUTPUT, "Probe X");
		configOutput(Y_OUTPUT, "Probe Y");
		configOutput(Z_OUTPUT, "Probe Z");
		publishDivider.setDivision(128);
	}

	double baseHz() { return params[RANGE_PARAM].getValue() > 0.5f ? dsp::FREQ_C4 : dsp::FREQ_C4 / 256; }

	cxo::OrbitState latest() const { return states[front.load(std::memory_order_acquire)]; }
	void publish(const cxo::OrbitState& s) {
		const int back = 1 - front.load(std::memory_order_relaxed);
		states[back] = s;
		front.store(back, std::memory_order_release);
	}

	void process(const ProcessArgs& args) override {
		int channels = 1;
		for (Input& in : inputs)
			channels = std::max(channels, in.getChannels());

		cxo::StockField field;
		field.kind = cxo::StockField::Kind(int(params[FIELD_PARAM].getValue()));
		field.octaves = int(params[OCTAVES_PARAM].getValue());
		field.seed = seed;
		const bool pinch = params[MODE_PARAM].getValue() > 0.5f;
		const bool probing = outputs[X_OUTPUT].isConnected() || outputs[Y_OUTPUT].isConnected() || outputs[Z_OUTPUT].isConnected();

		for (int c = 0; c < channels; ++c) {
			Voice& v = voices[c];
			const auto mod = [&](int id) {
				return double(params[id].getValue() + inputs[CV_INPUT + id].getPolyVoltage(c) * params[ATTEN_PARAM + id].getValue() * perVolt[id]);
			};
			const double hz = std::min(baseHz() * std::exp2(mod(FREQ_PARAM) + inputs[VOCT_INPUT].getPolyVoltage(c)), 0.45 * args.sampleRate);
			if (v.sync.process(inputs[SYNC_INPUT].getPolyVoltage(c), 0.1f, 1.f))
				v.phasor.phase = 0;
			v.drift += mod(DRIFT_PARAM) * args.sampleTime;

			const double tilt = mod(TILT_PARAM) * cxo::PI / 180, azimuth = mod(AZIMUTH_PARAM) * cxo::PI / 180;
			const cxo::Ring ring{{mod(X_PARAM), mod(Y_PARAM), mod(Z_PARAM) + v.drift},
			                     cxo::Ring::orient(mod(SPIN_PARAM) * cxo::PI / 180, tilt, azimuth),
			                     std::max(0.0, mod(MAJOR_PARAM)), std::max(0.0, mod(MINOR_PARAM))};
			const cxo::OrbitShape shape{ring, pinch, mod(SPREAD_PARAM), mod(ANGLE_PARAM) / 360, OUTS};

			unsigned wanted = 0;
			for (int k = 0; k < OUTS; ++k)
				wanted |= unsigned(outputs[WAVE_OUTPUT + k].isConnected()) << k;
			float out[OUTS];
			const double phase = v.process(shape, field, hz, args.sampleTime, center, normalize, out, wanted);
			for (int k = 0; k < OUTS; ++k)
				if (wanted >> k & 1)
					outputs[WAVE_OUTPUT + k].setVoltage(5.f * out[k], c);

			// The probe's coordinates are what sampling a linear ramp field along each axis would read.
			// Drift is left out so they stay within range however long it runs.
			if (probing) {
				const cxo::Vec3 p = shape.probe(0, phase);
				outputs[X_OUTPUT].setVoltage(float(p.x), c);
				outputs[Y_OUTPUT].setVoltage(float(p.y), c);
				outputs[Z_OUTPUT].setVoltage(float(p.z - v.drift), c);
			}

			if (c == 0 && publishDivider.process())
				publish({shape, tilt, azimuth, field, hz, phase, channels});
		}
		for (Output& out : outputs)
			out.setChannels(channels);
	}

	void onReset(const ResetEvent& e) override {
		Module::onReset(e);
		for (Voice& v : voices)
			v = Voice();
		seed = 0;
		center = normalize = autoRotate = showStrip = true;
	}

	void onRandomize(const RandomizeEvent& e) override {
		Module::onRandomize(e);
		seed = random::u32();
	}

	json_t* dataToJson() override {
		json_t* root = json_object();
		json_object_set_new(root, "seed", json_integer(seed));
		json_object_set_new(root, "center", json_boolean(center));
		json_object_set_new(root, "normalize", json_boolean(normalize));
		json_object_set_new(root, "autoRotate", json_boolean(autoRotate));
		json_object_set_new(root, "showStrip", json_boolean(showStrip));
		return root;
	}

	void dataFromJson(json_t* root) override {
		if (json_t* j = json_object_get(root, "seed"))
			seed = json_integer_value(j);
		for (auto [key, flag] : {std::pair{"center", &center}, {"normalize", &normalize}, {"autoRotate", &autoRotate}, {"showStrip", &showStrip}})
			if (json_t* j = json_object_get(root, key))
				*flag = json_boolean_value(j);
	}
};

struct OrbitDisplay : OledDisplay {
	Orbit* module = nullptr;
	cxo::OrbitScene scene;
	double visualPhase = 0, previewDrift = 0;
	bool dragging = false;

	cxo::OrbitState state() const {
		if (module)
			return module->latest();
		cxo::OrbitState s; // a slowly drifting stand-in for the module browser
		s.shape.ring.center.z = previewDrift;
		s.hz = 0.25;
		return s;
	}

	void step() override {
		const double dt = std::clamp(APP->window->getLastFrameDuration(), 0.0, 0.1);
		// Audio-rate phase only strobes at the frame rate, so fast oscillators are shown octave-folded
		// down to a watchable rotation that keeps their pitch class.
		double hz = state().hz;
		while (hz >= 0.5)
			hz *= 0.5;
		visualPhase += hz * dt;
		visualPhase -= std::floor(visualPhase);
		previewDrift += 0.15 * dt;
		if ((!module || module->autoRotate) && !dragging)
			scene.camAzimuth += 0.08 * dt;
		OledDisplay::step();
	}

	void paintScene(cxo::Painter& painter) override {
		cxo::OrbitState s = state();
		if (!module || s.hz > 2)
			s.phase = visualPhase;
		scene.strip = !module || module->showStrip;
		scene.paint(painter, s);
	}

	void onButton(const ButtonEvent& e) override {
		if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT)
			e.consume(this);
	}
	void onDragStart(const DragStartEvent& e) override { dragging = true; }
	void onDragEnd(const DragEndEvent& e) override { dragging = false; }
	void onDragMove(const DragMoveEvent& e) override {
		const float k = 0.01f / getAbsoluteZoom();
		scene.camAzimuth -= e.mouseDelta.x * k;
		scene.camElevation = std::clamp(scene.camElevation + e.mouseDelta.y * k, -1.4, 1.4);
	}
};

struct OrbitWidget : ModuleWidget {
	OrbitWidget(Orbit* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Orbit.svg")));
		auto* art = new PanelArt(box.size);
		addChild(art);

		addChild(createWidget<ThemedScrew>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ThemedScrew>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ThemedScrew>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ThemedScrew>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		art->text({13.f, 6.2f}, "ORBIT", 11.f, -1, true);
		art->text({139.4f, 6.2f}, "un1tz3r0", 7.f, 1);

		auto* display = createWidget<OrbitDisplay>(mm2px(Vec(5.f, 11.f)));
		display->box.size = mm2px(Vec(99.f, 45.f));
		display->module = module;
		addChild(display);

		art->card({107.f, 11.f}, {148.4f, 56.f});
		const std::pair<const char*, int> selectors[] = {
			{"FIELD", Orbit::FIELD_PARAM}, {"OCTAVES", Orbit::OCTAVES_PARAM}, {"MODE", Orbit::MODE_PARAM}, {"RANGE", Orbit::RANGE_PARAM}};
		for (int i = 0; i < 4; ++i) {
			const Vec at(117.5f + 20.5f * (i % 2), 25.f + 21.f * (i / 2));
			art->text(at.plus(Vec(0.f, -8.f)), selectors[i].first);
			addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(at), module, selectors[i].second));
		}

		art->card({3.f, 58.f}, {149.4f, 111.f});
		const char* names[Orbit::MODS] = {"FREQ", "X", "Y", "Z", "R MAJOR", "R MINOR", "DRIFT", "SPIN", "TILT", "AZIMUTH", "SPREAD", "ANGLE"};
		for (int i = 0; i < Orbit::MODS; ++i) {
			const Vec at(15.2f + 24.4f * (i % 6), i < 6 ? 68.5f : 94.5f);
			art->text(at.plus(Vec(0.f, -7.8f)), names[i]);
			addParam(createParamCentered<RoundBlackKnob>(mm2px(at), module, i));
			addParam(createParamCentered<Trimpot>(mm2px(at.plus(Vec(-5.6f, 11.f))), module, Orbit::ATTEN_PARAM + i));
			addInput(createInputCentered<ThemedPJ301MPort>(mm2px(at.plus(Vec(5.6f, 11.f))), module, Orbit::CV_INPUT + i));
		}

		art->card({12.f, 112.5f}, {37.5f, 127.5f});
		art->card({40.f, 112.5f}, {139.5f, 127.5f}, true);
		const auto jack = [&](float x, const char* name) {
			art->text({x, 115.2f}, name, 5.5f);
			return mm2px(Vec(x, 122.f));
		};
		addInput(createInputCentered<ThemedPJ301MPort>(jack(18.5f, "V/OCT"), module, Orbit::VOCT_INPUT));
		addInput(createInputCentered<ThemedPJ301MPort>(jack(31.f, "SYNC"), module, Orbit::SYNC_INPUT));
		for (int k = 0; k < Orbit::OUTS; ++k)
			addOutput(createOutputCentered<ThemedPJ301MPort>(jack(48.f + 13.f * k, string::f("OUT %d", k + 1).c_str()), module, Orbit::WAVE_OUTPUT + k));
		const char* axes[] = {"X", "Y", "Z"};
		for (int k = 0; k < 3; ++k)
			addOutput(createOutputCentered<ThemedPJ301MPort>(jack(106.f + 13.f * k, axes[k]), module, Orbit::X_OUTPUT + k));
	}

	void appendContextMenu(Menu* menu) override {
		Orbit* module = getModule<Orbit>();
		menu->addChild(new MenuSeparator);
		menu->addChild(createBoolPtrMenuItem("Remove DC offset", "", &module->center));
		menu->addChild(createBoolPtrMenuItem("Normalize level", "", &module->normalize));
		menu->addChild(createMenuItem("New noise seed", string::f("%lld", (long long)module->seed), [=] { module->seed = random::u32(); }));
		menu->addChild(new MenuSeparator);
		menu->addChild(createMenuLabel("Display"));
		menu->addChild(createBoolPtrMenuItem("Auto-rotate camera", "", &module->autoRotate));
		menu->addChild(createBoolPtrMenuItem("Waveform strip", "", &module->showStrip));
	}
};

Model* modelOrbit = createModel<Orbit, OrbitWidget>("Orbit");
