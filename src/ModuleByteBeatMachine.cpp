#include "plugin.hpp"
#include "duktape.h"



struct ModuleByteBeatMachine : Module {
	enum ParamIds {
		TRIGGER_PARAM,
		MODE_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		TRIGGER_INPUT,
		XIN_INPUT,
		YIN_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		OUTPUT_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		LEDERROR_LIGHT,
		LEDREADY_LIGHT,
		NUM_LIGHTS
	};
	char javascriptBuffer[65536];
	const char* header = "function f(t,X,Y)	{ return (";
	const char* footer = "); } ";
	double accumulator = 0.0f;
	double timestep = 1.0/8000.0f;
	TextField* textField=NULL;
	bool running = false;
	bool compiled = false;
	dsp::SchmittTrigger trigger;
	float phase = 0.0;
	float blinkPhase = 0.0;
	volatile int t = 0;
	volatile int tl = 0;
	volatile float xl = 0.0;
	volatile float yl = 0.0;
	volatile float output = 0.0f;	
	duk_context *ctx = NULL;

	ModuleByteBeatMachine() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(TRIGGER_PARAM, 0.f, 1.f, 0.f, "");
		configParam(MODE_PARAM, 0.f, 1.f, 1.f, "ByteBeat");
	}
	~ModuleByteBeatMachine()
	{
		if(ctx)
		{
			duk_destroy_heap(ctx);
		}
		if(!ctx)
		{
			ctx = NULL;
		}
	}
	void onAdd () override
	{
		running = false;
		compiled = false;
		
		if(ctx)
		{
			duk_destroy_heap(ctx);
			ctx = duk_create_heap_default();
		}
		else
		{
			ctx = duk_create_heap_default();
		}
		//textField->text = "t = t+1;";
		
		if(textField==NULL)
			return;
		if(strlen(textField->text.c_str())<512)
		{
			sprintf(javascriptBuffer,"%s%s%s",header,textField->text.c_str(),footer);
			if (duk_pcompile_string(ctx, DUK_COMPILE_FUNCTION,javascriptBuffer)==0)
			{
				compiled = true;
				t = 0;
				tl = 0;
				running = true;

			}
			else
			{
				compiled = false;
				running = false;
			}
		}
		else
		{
			compiled = false;
			running = false;
		}
	}
	/*
	void onDelete() override
	{
		if(ctx)
		{
			duk_destroy_heap(ctx);
		}
		if(!ctx)
		{
			ctx = NULL;
		}
	}
	void onReset () override
	{
		onCreate();
	}
	*/
	void process(const ProcessArgs &args) override
	{
		tl = 0;
		// Implement a bytebeat oscillator
		float deltaTime = args.sampleTime;
		int x = 0;
		int y = 0;
		float xf = 0.0;
		float yf = 0.0;
		trigger.process(rescale(inputs[TRIGGER_INPUT].value, 0.1f, 2.f, 0.f, 1.f));
		
		if( params[TRIGGER_PARAM].value && compiled)
		{
			t = 0;
			accumulator = 0.0f;
			running = true;	
		}
		if( inputs[TRIGGER_INPUT].value>0.0f && compiled)
		{
			t = 0;
			accumulator = 0.0f;
			running = true;	
		}
		xf = inputs[XIN_INPUT].value;
		yf = inputs[YIN_INPUT].value;
		if(params[MODE_PARAM].getValue() < 0.f)
		{

			x=(uint8_t)(((inputs[XIN_INPUT].value+5.0f)/10.0)*255); //scale -5.0 .. +5.0 to 0-255
			y=(uint8_t)(((inputs[YIN_INPUT].value+5.0f)/10.0)*255); //scale -5.0 .. +5.0 to 0-255
			/* code */
		}
		
		accumulator = accumulator + deltaTime;
		t = accumulator/timestep;
		int retval = 0;
		
		if(t>tl || tl==0 || xl!=xf || yl!=yf)
		{
			if(running)
			{
				if(params[MODE_PARAM].getValue() > 0.f)
				{
					duk_dup(ctx, 0);
					duk_push_number(ctx, (double)t);
					duk_push_number(ctx, xf);
					duk_push_number(ctx, yf);
					if(duk_pcall(ctx, 3)==0)
					{
						retval = duk_get_number_default(ctx, 1, 0.0);
					}
				
					duk_pop(ctx);
					output = retval;

				}
				else
				{
					duk_dup(ctx, 0);
					duk_push_int(ctx, t);
					duk_push_int(ctx, x);
					duk_push_int(ctx, y);
					if(duk_pcall(ctx, 3)==0)
					{
						retval = (uint8_t)duk_get_int_default(ctx, 1, 0);
					}
				
					duk_pop(ctx);
					output = 5.0f * ((retval-127.0)/127.0);
					/* code */
				}
						
				
			}
			
			/* code */
		}
		
		// Compute the output
		
		tl = t;
		xl = xf;
		yl = yf;
		outputs[OUTPUT_OUTPUT].value = output;
		lights[LEDREADY_LIGHT].value = std::max(inputs[TRIGGER_INPUT].value , params[TRIGGER_PARAM].value);
		// Blink light at 1Hz
		blinkPhase += deltaTime;
		if (blinkPhase >= 1.0f)
			blinkPhase -= 1.0f;
		if(compiled)
		{
			lights[LEDERROR_LIGHT].value = 1.0f;
		}
		else
		{
			lights[LEDERROR_LIGHT].value = (blinkPhase < 0.5f) ? 1.0f : 0.0f;
		}

	}
};

class BokontepTextField : public LedDisplayTextField  {
		public: 
			BokontepTextField() : LedDisplayTextField(){}
			virtual void onChange(const event::Change &e ) override;
			void setModule(ModuleByteBeatMachine* module)
			{
				m_module = module;
				if(m_module!=NULL)
				{
					m_module->textField = this;
				}
				
			}
		private:
			
			ModuleByteBeatMachine* m_module;

		
	};
	

void BokontepTextField::onChange(const event::Change&) {
	if(m_module)
	{
		m_module->onAdd();
	}
}

struct ModuleByteBeatMachineWidget : ModuleWidget {
	
	

	BokontepTextField *textField;
	
	ModuleByteBeatMachineWidget(ModuleByteBeatMachine *module) {
		if(module!=NULL)
		{
			setModule(module);
		}
		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/ModuleByteBeatMachine.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addParam(createParamCentered<LEDButton>(mm2px(Vec(13.185, 102.812)), module, ModuleByteBeatMachine::TRIGGER_PARAM));
		addParam(createParam<CKSS>(mm2px(Vec(36.5, 14.224)), module, ModuleByteBeatMachine::MODE_PARAM));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(13.332, 111.018)), module, ModuleByteBeatMachine::TRIGGER_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(29.3, 111.018)), module, ModuleByteBeatMachine::XIN_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(44.886, 111.018)), module, ModuleByteBeatMachine::YIN_INPUT));

		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(70.583, 111.018)), module, ModuleByteBeatMachine::OUTPUT_OUTPUT));

		addChild(createLightCentered<MediumLight<RedLight>>(mm2px(Vec(58.719, 17.224)), module, ModuleByteBeatMachine::LEDERROR_LIGHT));
		addChild(createLightCentered<MediumLight<GreenLight>>(mm2px(Vec(23.016, 17.224)), module, ModuleByteBeatMachine::LEDREADY_LIGHT));
		
		textField = createWidget<BokontepTextField>((mm2px(Vec(1, 22.504))));
		if(module)
		{
			textField->setModule(module);
		}
		textField->box.size = mm2px(Vec(78.480,77));
		textField->multiline = true;
		
		// mm2px(Vec(67.148, 76.125))
		addChild(textField);
	}
		json_t *toJson() override {
		json_t *rootJ = ModuleWidget::toJson();

		// text
		if(textField!=NULL)
		{
			json_object_set_new(rootJ, "text", json_string(textField->text.c_str()));
		}
		return rootJ;
	}

	void fromJson(json_t *rootJ) override {
		ModuleWidget::fromJson(rootJ);

		// text
		json_t *textJ = json_object_get(rootJ, "text");
		if (textJ)
		{
			if(textField)
			{
				textField->text = json_string_value(textJ);
			}
		}
	}
};


Model *modelModuleByteBeatMachine = createModel<ModuleByteBeatMachine, ModuleByteBeatMachineWidget>("ModuleByteBeatMachine");