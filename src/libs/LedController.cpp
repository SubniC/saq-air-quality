#include "LedController.h"
#include "neopixel.h"


LedController::LedController(uint8_t data_pin, uint8_t num_leds, uint8_t brightness) :
 _is_init(false),
 _brightness(brightness),
 _data_pin(data_pin),
 _num_leds(num_leds > MAX_LEDS ? MAX_LEDS : num_leds) {
    set_disabled(); //Inicializamos los workers apagados.

    _led_strip = new Adafruit_NeoPixel(_num_leds,_data_pin,WS2812B);
    _led_strip->begin();
    _led_strip->setBrightness(_brightness);
    _led_strip->show();
};

LedController::~LedController() {
}

bool LedController::setWorkingMode(LedWorkingModes mode)
{
    //TODO: Limpiar las cosas que procesa
    if(mode != activeMode)
    {
        //Apagamos todos los leds
        _led_strip->clear();
        _led_strip->show();
        //Aplicamos el nuevo estado
        activeMode = mode;
        return true;
    }
    return false;
}

void LedController::offAll()
{
    _led_strip->clear();
    _led_strip->show();
}

void LedController::loop()
{
    switch(activeMode)
    {
        case LedWorkingModes::PATTERN:
            _pattern_loop();
            break;
        default:
            _indivdual_loop();
    }
}

void LedController::_pattern_loop()
{
    if((millis() - currentPatternState.lastUpdate) > currentPatternState.interval) // time to update
    {
        currentPatternState.lastUpdate = millis();
        switch(currentPatternState.activePattern)
        {
            case SCANNER:
                _scannerUpdate();
                break;
            case FADE:
                _fadeUpdate();
                break;
            default:
                break;
        }
    }
}

void LedController::_indivdual_loop()
{
    bool updateLeds = false;
    if(millis() >= _led_next_update)
    {
        //Iteramos sobre los leds
        for (int j = 0; j < _num_leds; j++)
        {
            //si al led le toca update (o revision de estado)
            if(millis() >= _system_led_workers[j].next_update)// && _system_led_workers[j].mode != LedMode::OFF)
            {
                //dependiendo del tipo de led
                switch (_system_led_workers[j].mode)
                {
                    //el tipo de animacion es blink
                    case LedMode::BLINK:
                        //Comprobamos si nos quedan iteraciones, si iteration es -1 es bucle inficnito para el led
                        ///en caso contrario, si iterations es > currentIteration
                        if (_system_led_workers[j].iterations < 0 || (_system_led_workers[j].iterations > _system_led_workers[j].currentIteration))
                        {

                            // uint32_t dimmedColor1 = _dimColor(_system_led_workers[j].color1,(uint8_t)round(256/_brightness)-1);
#ifdef DEBUG_LED_CONTROLLER
              DBG_TAG("DEBUG", "[LED_CONTROLLER] @@@@@@@@");
              DBG_TAG("DEBUG", "[LED_CONTROLLER] PIXELC  [0x%x]",_led_strip->getPixelColor(j));
              DBG_TAG("DEBUG", "[LED_CONTROLLER] COLOR1  [0x%x]",_system_led_workers[j].color1);
              DBG_TAG("DEBUG", "[LED_CONTROLLER] COLOR2  [0x%x]",_system_led_workers[j].color2);
              DBG_TAG("DEBUG", "[LED_CONTROLLER] @@@@@@@@");
#endif
                            // if ( dimmedColor1 != _led_strip->getPixelColor(j))
                            if ( _system_led_workers[j].color1_time )
                            {
                                _led_strip->setPixelColor(j,_system_led_workers[j].color1);
                            }
                            else
                            {
                                _led_strip->setPixelColor(j,_system_led_workers[j].color2);
                            }
                            //Cambiamos de color2
                            _system_led_workers[j].color1_time = !_system_led_workers[j].color1_time;
                            //Incrementamos la iteracion del led
                            _system_led_workers[j].currentIteration++;
                        }
                        else
                        {
                            //Animacion finalizada
                            //la animacion tenia un numero maximo de iteraciones y se han superado, vamos a poner el led en estado fijo
                            //se aplicara el color3 que es el destinado a color final
                            set_fixed_led(j, _system_led_workers[j].color3);
                        }
                        break;
                    //Si el modo del led es fijo
                    case LedMode::FIXED:
                    //TODO: Ojo la comparacion no funciona si se dimea el brillo
                        //Miramos si ya esta del color deseado (black apra apagar)
                        if (_system_led_workers[j].color3 != _led_strip->getPixelColor(j))
                        {
                            //de no ser asi lo ponemos del color deseado
                            _led_strip->setPixelColor(j,_system_led_workers[j].color3);
                        }
                        break;
                }
                updateLeds = true;
                //Actualizamos el momento en el que volveremos a comprobar el led en funcion de su intervalo
                _system_led_workers[j].next_update = millis() + _system_led_workers[j].interval;
            }
        }

        if (updateLeds)
        {
           _led_strip->show();
        }

        _led_next_update = millis() + _indivual_mode_update_interval;
    }

}

// Increment the Index and reset at the end
void LedController::_increment()
{
    if (currentPatternState.direction == LedPatternDirection::FORWARD)
    {
       currentPatternState.index++;
       if (currentPatternState.index >= currentPatternState.totalSteps)
        {
            currentPatternState.index = 0;
            _on_pattern_finish(); // call the comlpetion callback
        }
    }
    else // Direction == REVERSE
    {
        --currentPatternState.index;
        if (currentPatternState.index <= 0)
        {
            currentPatternState.index = currentPatternState.totalSteps-1;
            _on_pattern_finish(); // call the comlpetion callback
        }
    }
}

void LedController::_on_pattern_finish()
{
    //Aqui llama cada vez que tenemos un patron finalizado
    //verificamos que seguimos teniendo tiempo y repetimo
    //o bien cancelamos el patron
    if(activeMode == LedWorkingModes::PATTERN)
    {
        if(millis() >= currentPatternState.maxDurationTill)
        {
            //HEmos terminado...
            setWorkingMode(LedWorkingModes::INDIVIDUAL);
        }
    }

}

// Reverse pattern direction
void LedController::_reverse()
{
    if (currentPatternState.direction == LedPatternDirection::FORWARD)
    {
        currentPatternState.direction = LedPatternDirection::REVERSE;
        currentPatternState.index = currentPatternState.totalSteps-1;
    }
    else
    {
        currentPatternState.direction = LedPatternDirection::FORWARD;
        currentPatternState.index = 0;
    }
}

// Initialize for a SCANNNER
void LedController::scanner(uint32_t color1, unsigned long duration)
{
    scanner(color1,50,duration);
}

void LedController::scanner(uint32_t color1, uint8_t interval, unsigned long duration)
{
    currentPatternState.activePattern = SCANNER;
    currentPatternState.interval = interval;
    currentPatternState.totalSteps = (_led_strip->numPixels() - 1) * 2;
    currentPatternState.color1 = color1;
    currentPatternState.index = 0;
    currentPatternState.maxDurationTill = millis() + duration;
    setWorkingMode(LedWorkingModes::PATTERN);
}

// Update the Scanner Pattern
void LedController::_scannerUpdate()
{
    for (int i = 0; i < _led_strip->numPixels(); i++)
    {
        if (i == currentPatternState.index)  // Scan Pixel to the right
        {
             _led_strip->setPixelColor(i, currentPatternState.color1);
        }
        else if (i == currentPatternState.totalSteps - currentPatternState.index) // Scan Pixel to the left
        {
             _led_strip->setPixelColor(i, currentPatternState.color1);
        }
        else // Fading tail
        {
             _led_strip->setPixelColor(i, _dimColor(_led_strip->getPixelColor(i)));
        }
    }
    _led_strip->show();
    _increment();
}

void LedController::fade(uint32_t color1, uint32_t color2, unsigned long duration)
{
    fade(color1,color2,8,50,LedPatternDirection::FORWARD,duration);
}

void LedController::fade(uint32_t color1, uint32_t color2, uint16_t steps, uint8_t interval, unsigned long duration)
{
    fade(color1,color2,steps,interval,LedPatternDirection::FORWARD,duration);
}

// Initialize for a Fade
void LedController::fade(uint32_t color1, uint32_t color2, uint16_t steps, uint8_t interval, LedPatternDirection dir, unsigned long duration)
{
    currentPatternState.activePattern = FADE;
    currentPatternState.interval = interval;
    currentPatternState.totalSteps = steps;
    currentPatternState.color1 = color1;
    currentPatternState.color2 = color2;
    currentPatternState.index = 0;
    currentPatternState.direction = dir;
    currentPatternState.maxDurationTill = millis() + duration;
    setWorkingMode(LedWorkingModes::PATTERN);
}

// Update the Fade Pattern
void LedController::_fadeUpdate()
{
    // Calculate linear interpolation between Color1 and Color2
    // Optimise order of operations to minimize truncation error
    uint8_t red = ((_red(currentPatternState.color1) * (currentPatternState.totalSteps - currentPatternState.index)) + (_red(currentPatternState.color2) * currentPatternState.index)) / currentPatternState.totalSteps;
    uint8_t green = ((_green(currentPatternState.color1) * (currentPatternState.totalSteps - currentPatternState.index)) + (_green(currentPatternState.color2) * currentPatternState.index)) / currentPatternState.totalSteps;
    uint8_t blue = ((_blue(currentPatternState.color1) * (currentPatternState.totalSteps - currentPatternState.index)) + (_blue(currentPatternState.color2) * currentPatternState.index)) / currentPatternState.totalSteps;

    _colorSet(_led_strip->Color(red, green, blue));
    // _led_strip->show();
    _increment();
}


// Calculate dimmed version of a color
uint32_t LedController::_dimColor(uint32_t color, uint8_t factor)
{
    // Shift R, G and B components one bit to the right
    uint32_t dimColor = _led_strip->Color(_red(color) >> factor, _green(color) >> factor, _blue(color) >> factor);
    return dimColor;
}

// Set all pixels to a color (synchronously)
void LedController::_colorSet(uint32_t color)
{
    for (int i = 0; i < _led_strip->numPixels(); i++)
    {
        _led_strip->setPixelColor(i, color);
    }
    _led_strip->show();
}

// Returns the Red component of a 32-bit color
uint8_t LedController::_red(uint32_t color)
{
    return (color >> 16) & 0xFF;
}

// Returns the Green component of a 32-bit color
uint8_t LedController::_green(uint32_t color)
{
    return (color >> 8) & 0xFF;
}

// Returns the Blue component of a 32-bit color
uint8_t LedController::_blue(uint32_t color)
{
    return color & 0xFF;
}

void LedController::set_disabled()
{
	for (int j = 0; j < _num_leds; j++)
	{
		//neopixel_array[j] = CRGB::Black;
		//set_fixed_led(j, NSFastLED::CRGB::Black);
        set_off_state(j);
	}
}

void LedController::onAll(uint32_t color)
{
    _colorSet(color);
}

void LedController::set_off_state(uint8_t id)
{
    set_fixed_led(id, HTMLColorCode::Black);
}

void LedController::set_fixed_led(uint8_t id, uint32_t color)
{
    _system_led_workers[id].mode = LedMode::FIXED;
    _system_led_workers[id].color1 = HTMLColorCode::Black; //Color incial
    _system_led_workers[id].color2 = HTMLColorCode::Black; //Color secundario para el parpadeo
    _system_led_workers[id].color3 = color; //Color final despues del parpadeo
    _system_led_workers[id].iterations = 0; //Numero de iteraciones, -1 para infinito 7realmente no vale mucho aqui
    _system_led_workers[id].interval = 500; //intervalo que no vale mucho
    _system_led_workers[id].currentIteration = 0; //Inicializamos el contador, que aqui nov ale mucho
    _system_led_workers[id].next_update = 0; //0 queremos que se ejecute lo antes posible!
}

void LedController::blink(uint8_t id, uint32_t color1,uint32_t color2, uint16_t interval, int16_t blinks, uint32_t endcolor)
{
	_system_led_workers[id].mode = LedMode::BLINK;
	_system_led_workers[id].color1 = color1;
	_system_led_workers[id].color2 = color2;
	_system_led_workers[id].color3 = endcolor;
	_system_led_workers[id].iterations = blinks*2; //Cada blink son 2 iterations
    _system_led_workers[id].color1_time = true; //Empecemos con el color uno
	_system_led_workers[id].interval = interval;
    _system_led_workers[id].currentIteration = 0;
    _system_led_workers[id].next_update = 0;
    setWorkingMode(LedWorkingModes::INDIVIDUAL);
}

void LedController::on(uint8_t pixel, uint32_t color, uint32_t duration)
{
    LedController::set_fixed_led(pixel, color);
    setWorkingMode(LedWorkingModes::INDIVIDUAL);
}

void LedController::off(uint8_t pixel)
{
    LedController::set_off_state(pixel);
    setWorkingMode(LedWorkingModes::INDIVIDUAL);
}

// void LedController::knightRider(uint16_t cycles, uint16_t speed, uint8_t width, uint32_t color) {
// 	uint32_t old_val[_num_leds]; // up to 256 lights!
// 								  // Larson time baby!
// 	for (uint16_t i = 0; i < cycles; i++) {
// 		for (int count = 1; count<_num_leds; count++) {
// 			_system_neopixel_leds[count]= color;
// 			old_val[count] = color;
// 			for (int x = count; x>0; x--) {
// 				old_val[x - 1] = dimColor(old_val[x - 1], width);
// 				_system_neopixel_leds[x - 1]= old_val[x - 1];
// 			}
// 			NSFastLED::FastLED.show();
// 			NSFastLED::FastLED.delay(speed);
// 		}
// 		for (int count = _num_leds - 1; count >= 0; count--) {
// 			_system_neopixel_leds[count] = color;
// 			old_val[count] = color;
// 			for (int x = count; x <= _num_leds; x++) {
// 				old_val[x - 1] = dimColor(old_val[x - 1], width);
// 				_system_neopixel_leds[x + 1] = old_val[x + 1];
// 			}
// 			NSFastLED::FastLED.show();
// 			NSFastLED::FastLED.delay(speed);
// 		}
// 	}
// }
//
// uint32_t LedController::dimColor(uint32_t color, uint8_t width) {
// 	return (((color & 0xFF0000) / width) & 0xFF0000) + (((color & 0x00FF00) / width) & 0x00FF00) + (((color & 0x0000FF) / width) & 0x0000FF);
// }
