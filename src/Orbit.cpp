#include <atomic>
#include <tuple>
#include "plugin.hpp"
#include "OledDisplay.hpp"
#include "PanelArt.hpp"
#include "core/OrbitScene.hpp"

using namespace cxo::orbit;

// Sweeps an elliptical ring through a 3D field and plays what it reads.
struct Orbit : Module {
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
	int accent = 0; // of cxo::Theme::ACCENTS
	int antialias = cxo::Antialias::AUTO;
	int side = cxo::OrbitScene::BOTH, floorStyle = cxo::OrbitView::GRID, floorDensity = 1, floorBrightness = 2, hueShift = 1;
	bool center = true, normalize = true, autoRotate = true, equalXY = false;

	// Channel 0's state for the display, double-buffered so the UI thread never reads a half-written one.
	cxo::OrbitState states[2];
	cxo::Vec3 lastCenter, velocity; // channel 0's center, and its velocity smoothed to a few hertz
	bool moving = false;            // whether lastCenter holds a center yet
	std::atomic<int> front{0};
	dsp::ClockDivider publishDivider;

	Orbit() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, 0);
		for (int i = 0; i < MODS; ++i) {
			const cxo::orbit::Mod& m = MOD[i];
			if (i == FREQ_PARAM)
				configParam<FreqQuantity>(i, m.min, m.max, m.def, m.name, m.unit);
			else
				configParam(i, m.min, m.max, m.def, m.name, m.unit, 0.f, m.scale);
			configParam(ATTEN_PARAM + i, -1.f, 1.f, 0.f, std::string(m.name) + " CV", "%", 0.f, 100.f);
			configInput(CV_INPUT + i, i == FREQ_PARAM ? "Exponential FM" : std::string(m.name) + " CV");
			perVolt[i] = i == FREQ_PARAM ? 1.f : (m.max - m.min) / 10.f;
		}
		configSwitch(FIELD_PARAM, 0.f, 3.f, 0.f, "Field", {"Simplex 3D", "Simplex 2D", "Gyroid", "Cellular"});
		configParam(OCTAVES_PARAM, 1.f, 4.f, 1.f, "Octaves")->snapEnabled = true;
		configSwitch(MODE_PARAM, 0.f, 4.f, 0.f, "Spread mode",
		             {"Phase: B reads the same ring further ahead", "Pinch: B's ring is scaled about phase 0", "Stack: B's ring is shifted along the normal",
		              "Radial: B's ring is scaled about the center", "Fan: B's ring is turned about the diameter through phase 0"});
		configSwitch(AXIS_PARAM, 0.f, 5.f, 0.f, "Rotation axis", {"Normal (spin in plane)", "Major axis", "Minor axis", "World X", "World Y", "World Z"});
		configSwitch(RANGE_PARAM, 0.f, 1.f, 1.f, "Range", {"LFO", "Audio"});
		configInput(VOCT_INPUT, "1V/octave pitch");
		configInput(SYNC_INPUT, "Hard sync");
		configOutput(WAVE_OUTPUT, "A");
		configOutput(WAVE_OUTPUT + 1, "B");
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
		field.detail = params[OCTAVES_PARAM].getValue() - 1;
		field.seed = seed;
		const auto mode = cxo::OrbitShape::Mode(int(params[MODE_PARAM].getValue()));
		const auto axis = cxo::Ring::Axis(int(params[AXIS_PARAM].getValue()));
		const bool probing = outputs[X_OUTPUT].isConnected() || outputs[Y_OUTPUT].isConnected() || outputs[Z_OUTPUT].isConnected();
		const bool audio = params[RANGE_PARAM].getValue() > 0.5f;

		for (int c = 0; c < channels; ++c) {
			Voice& v = voices[c];
			v.setAntialias(cxo::Antialias::Level(antialias));
			const auto mod = [&](int id) {
				return double(params[id].getValue() + inputs[CV_INPUT + id].getPolyVoltage(c) * params[ATTEN_PARAM + id].getValue() * perVolt[id]);
			};
			const double hz = std::min(baseHz() * std::exp2(mod(FREQ_PARAM) + inputs[VOCT_INPUT].getPolyVoltage(c)), 0.45 * args.sampleRate);
			if (v.sync.process(inputs[SYNC_INPUT].getPolyVoltage(c), 0.1f, 1.f))
				v.phasor.phase = 0;
			v.drift += mod(DRIFT_PARAM) * args.sampleTime;
			v.turn += mod(ROTATE_PARAM) * args.sampleTime;
			v.turn -= std::floor(v.turn);

			const double pitch = mod(PITCH_PARAM) * cxo::PI / 180, roll = mod(ROLL_PARAM) * cxo::PI / 180;
			const cxo::Ring ring = cxo::orbitRing({mod(X_PARAM), mod(Y_PARAM), mod(Z_PARAM) + v.drift}, std::max(0.0, mod(RADIUS_PARAM)),
			                                      std::clamp(mod(ECCENTRICITY_PARAM), -1.0, 1.0), pitch, roll, axis, v.turn);
			const cxo::OrbitShape shape{ring, mode, std::clamp(mod(SPREAD_PARAM), -1.0, 1.0)};
			if (c == 0) {
				const double k = std::min(1.0, cxo::TAU * 4 * args.sampleTime);
				if (moving)
					velocity = velocity + ((ring.center - lastCenter) * args.sampleRate - velocity) * k;
				lastCenter = ring.center;
				moving = true;
			}

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

			if (c == 0 && publishDivider.process()) {
				cxo::OrbitState s{shape, pitch, roll, axis, field, hz, phase, velocity, channels, audio};
				for (int i = 0; i < MODS; ++i)
					s.values[i] = i == FREQ_PARAM ? float(baseHz() * std::exp2(mod(i))) : float(mod(i)) * MOD[i].scale;
				publish(s);
			}
		}
		for (Output& out : outputs)
			out.setChannels(channels);
	}

	void onReset(const ResetEvent& e) override {
		Module::onReset(e);
		for (Voice& v : voices)
			v = Voice();
		seed = 0;
		accent = 0;
		antialias = cxo::Antialias::AUTO;
		side = cxo::OrbitScene::BOTH;
		floorStyle = cxo::OrbitView::GRID;
		floorDensity = 1;
		floorBrightness = 2;
		hueShift = 1;
		moving = false;
		velocity = {};
		center = normalize = autoRotate = true;
		equalXY = false;
	}

	void onRandomize(const RandomizeEvent& e) override {
		Module::onRandomize(e);
		seed = random::u32();
	}

	json_t* dataToJson() override {
		json_t* root = json_object();
		json_object_set_new(root, "seed", json_integer(seed));
		json_object_set_new(root, "accent", json_integer(accent));
		json_object_set_new(root, "antialias", json_integer(antialias));
		json_object_set_new(root, "center", json_boolean(center));
		json_object_set_new(root, "normalize", json_boolean(normalize));
		json_object_set_new(root, "autoRotate", json_boolean(autoRotate));
		json_object_set_new(root, "side", json_integer(side));
		json_object_set_new(root, "floor", json_integer(floorStyle));
		json_object_set_new(root, "floorDensity", json_integer(floorDensity));
		json_object_set_new(root, "floorBrightness", json_integer(floorBrightness));
		json_object_set_new(root, "hueShift", json_integer(hueShift));
		json_object_set_new(root, "equalXY", json_boolean(equalXY));
		return root;
	}

	void dataFromJson(json_t* root) override {
		if (json_t* j = json_object_get(root, "seed"))
			seed = json_integer_value(j);
		if (json_t* j = json_object_get(root, "accent"))
			accent = json_integer_value(j);
		if (json_t* j = json_object_get(root, "antialias"))
			antialias = std::clamp(int(json_integer_value(j)), 0, cxo::Antialias::LEVELS - 1);
		for (auto [key, index, count] : {std::tuple{"side", &side, int(cxo::OrbitScene::SIDES)}, {"floor", &floorStyle, int(cxo::OrbitView::FLOORS)},
		                                 {"floorDensity", &floorDensity, cxo::OrbitView::DENSITIES},
		                                 {"floorBrightness", &floorBrightness, cxo::OrbitView::BRIGHTNESSES}, {"hueShift", &hueShift, cxo::OrbitView::HUES}})
			if (json_t* j = json_object_get(root, key))
				*index = std::clamp(int(json_integer_value(j)), 0, count - 1);
		for (auto [key, flag] : {std::pair{"center", &center}, {"normalize", &normalize}, {"autoRotate", &autoRotate}, {"equalXY", &equalXY}})
			if (json_t* j = json_object_get(root, key))
				*flag = json_boolean_value(j);
	}
};

struct OrbitDisplay : OledDisplay {
	Orbit* module = nullptr;
	cxo::OrbitScene scene;
	static constexpr double PREVIEW_DRIFT = 0.15; // units per second
	double visualPhase = 0, previewDrift = 0;
	bool dragging = false;

	cxo::OrbitState state() const {
		if (module)
			return module->latest();
		cxo::OrbitState s; // a slowly drifting stand-in for the module browser
		s.shape.ring.center.z = previewDrift;
		s.velocity.z = PREVIEW_DRIFT;
		return s;
	}

	void step() override {
		theme = scene.view.theme = cxo::Theme::accent(module ? module->accent : 0);
		const double dt = std::clamp(APP->window->getLastFrameDuration(), 0.0, 0.1);
		// Audio-rate phase only strobes at the frame rate, so fast oscillators are shown octave-folded
		// down to a watchable rotation that keeps their pitch class.
		double hz = state().hz;
		while (hz >= 0.5)
			hz *= 0.5;
		visualPhase += hz * dt;
		visualPhase -= std::floor(visualPhase);
		previewDrift += PREVIEW_DRIFT * dt;
		if ((!module || module->autoRotate) && !dragging)
			scene.camAzimuth += 0.08 * dt;
		OledDisplay::step();
	}

	void paintScene(cxo::Painter& painter) override {
		cxo::OrbitState s = state();
		if (!module || s.hz > 2)
			s.phase = visualPhase;
		scene.side = cxo::OrbitScene::Side(module ? module->side : int(cxo::OrbitScene::BOTH));
		scene.equalXY = module && module->equalXY;
		if (module) {
			scene.view.floor = cxo::OrbitView::Floor(module->floorStyle);
			scene.view.density = module->floorDensity;
			scene.view.brightness = module->floorBrightness;
			scene.view.hue = module->hueShift;
		}
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
		const cxo::Panel panel = cxo::orbit::panel();
		addPanel(this, panel);

		const cxo::Panel::Rect& glass = panel.screen;
		auto* display = createWidget<OrbitDisplay>(mm2px(Vec(glass.min.x, glass.min.y)));
		display->box.size = mm2px(Vec(glass.max.x - glass.min.x, glass.max.y - glass.min.y));
		display->module = module;
		display->scene.taps = panel.taps;
		addChild(display);
	}

	void appendContextMenu(Menu* menu) override {
		Orbit* module = getModule<Orbit>();
		menu->addChild(new MenuSeparator);
		menu->addChild(createBoolPtrMenuItem("Remove DC offset", "", &module->center));
		menu->addChild(createBoolPtrMenuItem("Normalize level", "", &module->normalize));
		menu->addChild(createIndexPtrSubmenuItem("Anti-aliasing", std::vector<std::string>(std::begin(cxo::Antialias::NAMES), std::end(cxo::Antialias::NAMES)), &module->antialias));
		menu->addChild(createMenuItem("New noise seed", string::f("%lld", (long long)module->seed), [=] { module->seed = random::u32(); }));
		menu->addChild(new MenuSeparator);
		menu->addChild(createMenuLabel("Display"));
		menu->addChild(createIndexPtrSubmenuItem("Color", std::vector<std::string>(std::begin(cxo::Theme::ACCENTS), std::end(cxo::Theme::ACCENTS)), &module->accent));
		menu->addChild(createBoolPtrMenuItem("Auto-rotate camera", "", &module->autoRotate));
		menu->addChild(createIndexPtrSubmenuItem("Side panel", std::vector<std::string>(std::begin(cxo::OrbitScene::SIDE_NAMES), std::end(cxo::OrbitScene::SIDE_NAMES)), &module->side));
		menu->addChild(createBoolPtrMenuItem("Equal XY scaling", "", &module->equalXY));
		const auto names = [](const auto& list) { return std::vector<std::string>(std::begin(list), std::end(list)); };
		menu->addChild(createIndexPtrSubmenuItem("Floor", names(cxo::OrbitView::FLOOR_NAMES), &module->floorStyle));
		menu->addChild(createIndexPtrSubmenuItem("Floor density", names(cxo::OrbitView::DENSITY_NAMES), &module->floorDensity));
		menu->addChild(createIndexPtrSubmenuItem("Floor brightness", names(cxo::OrbitView::BRIGHTNESS_NAMES), &module->floorBrightness));
		menu->addChild(createIndexPtrSubmenuItem("Hue shift with height", names(cxo::OrbitView::HUE_NAMES), &module->hueShift));
	}
};

Model* modelOrbit = createModel<Orbit, OrbitWidget>("Orbit");
