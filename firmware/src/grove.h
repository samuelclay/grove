#include <OctoWS2811.h>
#include <avr/power.h>
#include <SPI.h>

// ===================
// = Pin Definitions =
// ===================

const uint8_t treeAPin = 14;
const int slaveSelectPin = 10;

// ===========
// = Globals =
// ===========

// A single 1m strip is 144 LEDs/m. 720 = 5m.
// const int ledsPerStrip = 720;
const int ledsPerStrip = 428;

DMAMEM int displayMemory[ledsPerStrip*6];
int drawingMemory[ledsPerStrip*6];
const int config = WS2811_GRB | WS2811_800kHz;

// Number of LEDs for each drip.
const int REST_DRIP_WIDTH_MIN = 7;
const int REST_DRIP_WIDTH_MAX = 20;

// Amount of color fade from front-to-back of each drip.
// Can probably increase this to 0.64 during Burning Man.
const double REST_DRIP_DECAY = 0.56;
const double BREATH_DECAY = 0.56;

// Time taken for drip to traverse LEDs
const unsigned long REST_DRIP_TRIP_MS = 30000;
const unsigned long BREATH_TRIP_MS = 15000;

uint32_t dripCount = 0;
uint32_t breathCount = 0;

// This limit is responsible for how much memory the drips take.
const int DRIP_LIMIT = 20;
unsigned int dripStarts[DRIP_LIMIT];
unsigned int dripColors[DRIP_LIMIT];
unsigned int dripWidth[DRIP_LIMIT];

const int BREATH_LIMIT = 20;
unsigned int breathStarts[BREATH_LIMIT];
unsigned int breathWidth[BREATH_LIMIT];
unsigned int breathPosition[BREATH_LIMIT];
int activeBreath = -1;
unsigned int lastNewBreathMs = 0;
unsigned int endActiveBreathMs = 0;
unsigned int furthestBreathPosition = 0;
unsigned int newestBreathPosition = 0;

// High current Leaves
const int LEAVES_REST_MS = 2000;

// ========
// = LEDs =
// ========

OctoWS2811 leds(ledsPerStrip, displayMemory, drawingMemory, config);

// ========
// = SPI =
// ========

SPISettings dispatcherSPISettings(1e6, MSBFIRST, SPI_MODE3); 


// =============
// = Functions =
// =============

void drawDrip(int d, int dripStart, int color);
void drawBreath(int b, int breathStart);
void addRandomDrip();
void addBreath();
void advanceRestDrips();
void advanceBreaths();
int randomGreen();
void dispatcher(uint8_t chan, uint8_t value);
uint8_t flipByte(uint8_t val);
void runLeaves();

/**   
* \par   
* Sine Table is generated from following loop   
* <pre>for(i = 0; i < 360; i++)   
* {   
*    sinTable[i]= sin((i-180) * PI/180.0);   
* } </pre>   
*/


static const float sinTable[360] = {
  -0.017452406437283439f, -0.034899496702500699f, -0.052335956242943807f,
  -0.069756473744125524f, -0.087155742747658638f, -0.104528463267653730f,
  -0.121869343405147550f, -0.139173100960065740f,
  -0.156434465040230980f, -0.173648177666930280f, -0.190808995376544970f,
  -0.207911690817759310f, -0.224951054343864780f, -0.241921895599667730f,
  -0.258819045102521020f, -0.275637355816999660f,
  -0.292371704722737050f, -0.309016994374947510f, -0.325568154457156980f,
  -0.342020143325668880f, -0.358367949545300210f, -0.374606593415912240f,
  -0.390731128489274160f, -0.406736643075800430f,
  -0.422618261740699500f, -0.438371146789077290f, -0.453990499739546860f,
  -0.469471562785891080f, -0.484809620246337170f, -0.499999999999999940f,
  -0.515038074910054380f, -0.529919264233204900f,
  -0.544639035015026860f, -0.559192903470746900f, -0.573576436351046380f,
  -0.587785252292473250f, -0.601815023152048160f, -0.615661475325658400f,
  -0.629320391049837720f, -0.642787609686539470f,
  -0.656059028990507280f, -0.669130606358858350f, -0.681998360062498590f,
  -0.694658370458997140f, -0.707106781186547570f, -0.719339800338651410f,
  -0.731353701619170570f, -0.743144825477394240f,
  -0.754709580222771790f, -0.766044443118978010f, -0.777145961456971010f,
  -0.788010753606722010f, -0.798635510047292720f, -0.809016994374947450f,
  -0.819152044288992020f, -0.829037572555041740f,
  -0.838670567945424050f, -0.848048096156426070f, -0.857167300702112330f,
  -0.866025403784438710f, -0.874619707139395850f, -0.882947592858927100f,
  -0.891006524188367900f, -0.898794046299166930f,
  -0.906307787036650050f, -0.913545457642600980f, -0.920504853452440370f,
  -0.927183854566787420f, -0.933580426497201740f, -0.939692620785908430f,
  -0.945518575599316850f, -0.951056516295153640f,
  -0.956304755963035550f, -0.961261695938318890f, -0.965925826289068310f,
  -0.970295726275996470f, -0.974370064785235250f, -0.978147600733805690f,
  -0.981627183447663980f, -0.984807753012208020f,
  -0.987688340595137660f, -0.990268068741570360f, -0.992546151641322090f,
  -0.994521895368273400f, -0.996194698091745550f, -0.997564050259824200f,
  -0.998629534754573830f, -0.999390827019095760f,
  -0.999847695156391270f, -1.000000000000000000f, -0.999847695156391270f,
  -0.999390827019095760f, -0.998629534754573830f, -0.997564050259824200f,
  -0.996194698091745550f, -0.994521895368273290f,
  -0.992546151641321980f, -0.990268068741570250f, -0.987688340595137770f,
  -0.984807753012208020f, -0.981627183447663980f, -0.978147600733805580f,
  -0.974370064785235250f, -0.970295726275996470f,
  -0.965925826289068310f, -0.961261695938318890f, -0.956304755963035440f,
  -0.951056516295153530f, -0.945518575599316740f, -0.939692620785908320f,
  -0.933580426497201740f, -0.927183854566787420f,
  -0.920504853452440260f, -0.913545457642600870f, -0.906307787036649940f,
  -0.898794046299167040f, -0.891006524188367790f, -0.882947592858926880f,
  -0.874619707139395740f, -0.866025403784438600f,
  -0.857167300702112220f, -0.848048096156426070f, -0.838670567945423940f,
  -0.829037572555041740f, -0.819152044288991800f, -0.809016994374947450f,
  -0.798635510047292830f, -0.788010753606722010f,
  -0.777145961456970790f, -0.766044443118978010f, -0.754709580222772010f,
  -0.743144825477394240f, -0.731353701619170460f, -0.719339800338651080f,
  -0.707106781186547460f, -0.694658370458997250f,
  -0.681998360062498480f, -0.669130606358858240f, -0.656059028990507160f,
  -0.642787609686539250f, -0.629320391049837390f, -0.615661475325658180f,
  -0.601815023152048270f, -0.587785252292473140f,
  -0.573576436351046050f, -0.559192903470746900f, -0.544639035015027080f,
  -0.529919264233204900f, -0.515038074910054160f, -0.499999999999999940f,
  -0.484809620246337060f, -0.469471562785890810f,
  -0.453990499739546750f, -0.438371146789077400f, -0.422618261740699440f,
  -0.406736643075800150f, -0.390731128489273720f, -0.374606593415912010f,
  -0.358367949545300270f, -0.342020143325668710f,
  -0.325568154457156640f, -0.309016994374947400f, -0.292371704722736770f,
  -0.275637355816999160f, -0.258819045102520740f, -0.241921895599667730f,
  -0.224951054343865000f, -0.207911690817759310f,
  -0.190808995376544800f, -0.173648177666930330f, -0.156434465040230870f,
  -0.139173100960065440f, -0.121869343405147480f, -0.104528463267653460f,
  -0.087155742747658166f, -0.069756473744125302f,
  -0.052335956242943828f, -0.034899496702500969f, -0.017452406437283512f,
  0.000000000000000000f, 0.017452406437283512f, 0.034899496702500969f,
  0.052335956242943828f, 0.069756473744125302f,
  0.087155742747658166f, 0.104528463267653460f, 0.121869343405147480f,
  0.139173100960065440f, 0.156434465040230870f, 0.173648177666930330f,
  0.190808995376544800f, 0.207911690817759310f,
  0.224951054343865000f, 0.241921895599667730f, 0.258819045102520740f,
  0.275637355816999160f, 0.292371704722736770f, 0.309016994374947400f,
  0.325568154457156640f, 0.342020143325668710f,
  0.358367949545300270f, 0.374606593415912010f, 0.390731128489273720f,
  0.406736643075800150f, 0.422618261740699440f, 0.438371146789077400f,
  0.453990499739546750f, 0.469471562785890810f,
  0.484809620246337060f, 0.499999999999999940f, 0.515038074910054160f,
  0.529919264233204900f, 0.544639035015027080f, 0.559192903470746900f,
  0.573576436351046050f, 0.587785252292473140f,
  0.601815023152048270f, 0.615661475325658180f, 0.629320391049837390f,
  0.642787609686539250f, 0.656059028990507160f, 0.669130606358858240f,
  0.681998360062498480f, 0.694658370458997250f,
  0.707106781186547460f, 0.719339800338651080f, 0.731353701619170460f,
  0.743144825477394240f, 0.754709580222772010f, 0.766044443118978010f,
  0.777145961456970790f, 0.788010753606722010f,
  0.798635510047292830f, 0.809016994374947450f, 0.819152044288991800f,
  0.829037572555041740f, 0.838670567945423940f, 0.848048096156426070f,
  0.857167300702112220f, 0.866025403784438600f,
  0.874619707139395740f, 0.882947592858926880f, 0.891006524188367790f,
  0.898794046299167040f, 0.906307787036649940f, 0.913545457642600870f,
  0.920504853452440260f, 0.927183854566787420f,
  0.933580426497201740f, 0.939692620785908320f, 0.945518575599316740f,
  0.951056516295153530f, 0.956304755963035440f, 0.961261695938318890f,
  0.965925826289068310f, 0.970295726275996470f,
  0.974370064785235250f, 0.978147600733805580f, 0.981627183447663980f,
  0.984807753012208020f, 0.987688340595137770f, 0.990268068741570250f,
  0.992546151641321980f, 0.994521895368273290f,
  0.996194698091745550f, 0.997564050259824200f, 0.998629534754573830f,
  0.999390827019095760f, 0.999847695156391270f, 1.000000000000000000f,
  0.999847695156391270f, 0.999390827019095760f,
  0.998629534754573830f, 0.997564050259824200f, 0.996194698091745550f,
  0.994521895368273400f, 0.992546151641322090f, 0.990268068741570360f,
  0.987688340595137660f, 0.984807753012208020f,
  0.981627183447663980f, 0.978147600733805690f, 0.974370064785235250f,
  0.970295726275996470f, 0.965925826289068310f, 0.961261695938318890f,
  0.956304755963035550f, 0.951056516295153640f,
  0.945518575599316850f, 0.939692620785908430f, 0.933580426497201740f,
  0.927183854566787420f, 0.920504853452440370f, 0.913545457642600980f,
  0.906307787036650050f, 0.898794046299166930f,
  0.891006524188367900f, 0.882947592858927100f, 0.874619707139395850f,
  0.866025403784438710f, 0.857167300702112330f, 0.848048096156426070f,
  0.838670567945424050f, 0.829037572555041740f,
  0.819152044288992020f, 0.809016994374947450f, 0.798635510047292720f,
  0.788010753606722010f, 0.777145961456971010f, 0.766044443118978010f,
  0.754709580222771790f, 0.743144825477394240f,
  0.731353701619170570f, 0.719339800338651410f, 0.707106781186547570f,
  0.694658370458997140f, 0.681998360062498590f, 0.669130606358858350f,
  0.656059028990507280f, 0.642787609686539470f,
  0.629320391049837720f, 0.615661475325658400f, 0.601815023152048160f,
  0.587785252292473250f, 0.573576436351046380f, 0.559192903470746900f,
  0.544639035015026860f, 0.529919264233204900f,
  0.515038074910054380f, 0.499999999999999940f, 0.484809620246337170f,
  0.469471562785891080f, 0.453990499739546860f, 0.438371146789077290f,
  0.422618261740699500f, 0.406736643075800430f,
  0.390731128489274160f, 0.374606593415912240f, 0.358367949545300210f,
  0.342020143325668880f, 0.325568154457156980f, 0.309016994374947510f,
  0.292371704722737050f, 0.275637355816999660f,
  0.258819045102521020f, 0.241921895599667730f, 0.224951054343864780f,
  0.207911690817759310f, 0.190808995376544970f, 0.173648177666930280f,
  0.156434465040230980f, 0.139173100960065740f,
  0.121869343405147550f, 0.104528463267653730f, 0.087155742747658638f,
  0.069756473744125524f, 0.052335956242943807f, 0.034899496702500699f,
  0.017452406437283439f, 0.000000000000000122f
};


/**   
* \par   
* Cosine Table is generated from following loop   
* <pre>for(i = 0; i < 360; i++)   
* {   
*    cosTable[i]= cos((i-180) * PI/180.0);   
* } </pre>  
*/

static const float cosTable[360] = {
  -0.999847695156391270f, -0.999390827019095760f, -0.998629534754573830f,
  -0.997564050259824200f, -0.996194698091745550f, -0.994521895368273290f,
  -0.992546151641321980f, -0.990268068741570250f,
  -0.987688340595137660f, -0.984807753012208020f, -0.981627183447663980f,
  -0.978147600733805690f, -0.974370064785235250f, -0.970295726275996470f,
  -0.965925826289068200f, -0.961261695938318670f,
  -0.956304755963035440f, -0.951056516295153530f, -0.945518575599316740f,
  -0.939692620785908320f, -0.933580426497201740f, -0.927183854566787310f,
  -0.920504853452440150f, -0.913545457642600760f,
  -0.906307787036649940f, -0.898794046299167040f, -0.891006524188367790f,
  -0.882947592858926770f, -0.874619707139395740f, -0.866025403784438710f,
  -0.857167300702112220f, -0.848048096156425960f,
  -0.838670567945424160f, -0.829037572555041620f, -0.819152044288991580f,
  -0.809016994374947340f, -0.798635510047292940f, -0.788010753606721900f,
  -0.777145961456970680f, -0.766044443118977900f,
  -0.754709580222772010f, -0.743144825477394130f, -0.731353701619170460f,
  -0.719339800338651300f, -0.707106781186547460f, -0.694658370458997030f,
  -0.681998360062498370f, -0.669130606358858240f,
  -0.656059028990507500f, -0.642787609686539360f, -0.629320391049837280f,
  -0.615661475325658290f, -0.601815023152048380f, -0.587785252292473030f,
  -0.573576436351045830f, -0.559192903470746680f,
  -0.544639035015027080f, -0.529919264233204790f, -0.515038074910054270f,
  -0.499999999999999780f, -0.484809620246337000f, -0.469471562785890530f,
  -0.453990499739546750f, -0.438371146789077510f,
  -0.422618261740699330f, -0.406736643075800100f, -0.390731128489273600f,
  -0.374606593415912070f, -0.358367949545300270f, -0.342020143325668710f,
  -0.325568154457156420f, -0.309016994374947340f,
  -0.292371704722736660f, -0.275637355816999050f, -0.258819045102520850f,
  -0.241921895599667790f, -0.224951054343864810f, -0.207911690817759120f,
  -0.190808995376544800f, -0.173648177666930300f,
  -0.156434465040231040f, -0.139173100960065350f, -0.121869343405147370f,
  -0.104528463267653330f, -0.087155742747658235f, -0.069756473744125330f,
  -0.052335956242943620f, -0.034899496702500733f,
  -0.017452406437283477f, 0.000000000000000061f, 0.017452406437283376f,
  0.034899496702501080f, 0.052335956242943966f, 0.069756473744125455f,
  0.087155742747658138f, 0.104528463267653460f,
  0.121869343405147490f, 0.139173100960065690f, 0.156434465040230920f,
  0.173648177666930410f, 0.190808995376544920f, 0.207911690817759450f,
  0.224951054343864920f, 0.241921895599667900f,
  0.258819045102520740f, 0.275637355816999160f, 0.292371704722736770f,
  0.309016994374947450f, 0.325568154457156760f, 0.342020143325668820f,
  0.358367949545300380f, 0.374606593415911960f,
  0.390731128489273940f, 0.406736643075800210f, 0.422618261740699440f,
  0.438371146789077460f, 0.453990499739546860f, 0.469471562785890860f,
  0.484809620246337110f, 0.500000000000000110f,
  0.515038074910054380f, 0.529919264233204900f, 0.544639035015027200f,
  0.559192903470746790f, 0.573576436351046050f, 0.587785252292473140f,
  0.601815023152048270f, 0.615661475325658290f,
  0.629320391049837500f, 0.642787609686539360f, 0.656059028990507280f,
  0.669130606358858240f, 0.681998360062498480f, 0.694658370458997370f,
  0.707106781186547570f, 0.719339800338651190f,
  0.731353701619170570f, 0.743144825477394240f, 0.754709580222772010f,
  0.766044443118978010f, 0.777145961456970900f, 0.788010753606722010f,
  0.798635510047292830f, 0.809016994374947450f,
  0.819152044288991800f, 0.829037572555041620f, 0.838670567945424050f,
  0.848048096156425960f, 0.857167300702112330f, 0.866025403784438710f,
  0.874619707139395740f, 0.882947592858926990f,
  0.891006524188367900f, 0.898794046299167040f, 0.906307787036649940f,
  0.913545457642600870f, 0.920504853452440370f, 0.927183854566787420f,
  0.933580426497201740f, 0.939692620785908430f,
  0.945518575599316850f, 0.951056516295153530f, 0.956304755963035440f,
  0.961261695938318890f, 0.965925826289068310f, 0.970295726275996470f,
  0.974370064785235250f, 0.978147600733805690f,
  0.981627183447663980f, 0.984807753012208020f, 0.987688340595137770f,
  0.990268068741570360f, 0.992546151641321980f, 0.994521895368273290f,
  0.996194698091745550f, 0.997564050259824200f,
  0.998629534754573830f, 0.999390827019095760f, 0.999847695156391270f,
  1.000000000000000000f, 0.999847695156391270f, 0.999390827019095760f,
  0.998629534754573830f, 0.997564050259824200f,
  0.996194698091745550f, 0.994521895368273290f, 0.992546151641321980f,
  0.990268068741570360f, 0.987688340595137770f, 0.984807753012208020f,
  0.981627183447663980f, 0.978147600733805690f,
  0.974370064785235250f, 0.970295726275996470f, 0.965925826289068310f,
  0.961261695938318890f, 0.956304755963035440f, 0.951056516295153530f,
  0.945518575599316850f, 0.939692620785908430f,
  0.933580426497201740f, 0.927183854566787420f, 0.920504853452440370f,
  0.913545457642600870f, 0.906307787036649940f, 0.898794046299167040f,
  0.891006524188367900f, 0.882947592858926990f,
  0.874619707139395740f, 0.866025403784438710f, 0.857167300702112330f,
  0.848048096156425960f, 0.838670567945424050f, 0.829037572555041620f,
  0.819152044288991800f, 0.809016994374947450f,
  0.798635510047292830f, 0.788010753606722010f, 0.777145961456970900f,
  0.766044443118978010f, 0.754709580222772010f, 0.743144825477394240f,
  0.731353701619170570f, 0.719339800338651190f,
  0.707106781186547570f, 0.694658370458997370f, 0.681998360062498480f,
  0.669130606358858240f, 0.656059028990507280f, 0.642787609686539360f,
  0.629320391049837500f, 0.615661475325658290f,
  0.601815023152048270f, 0.587785252292473140f, 0.573576436351046050f,
  0.559192903470746790f, 0.544639035015027200f, 0.529919264233204900f,
  0.515038074910054380f, 0.500000000000000110f,
  0.484809620246337110f, 0.469471562785890860f, 0.453990499739546860f,
  0.438371146789077460f, 0.422618261740699440f, 0.406736643075800210f,
  0.390731128489273940f, 0.374606593415911960f,
  0.358367949545300380f, 0.342020143325668820f, 0.325568154457156760f,
  0.309016994374947450f, 0.292371704722736770f, 0.275637355816999160f,
  0.258819045102520740f, 0.241921895599667900f,
  0.224951054343864920f, 0.207911690817759450f, 0.190808995376544920f,
  0.173648177666930410f, 0.156434465040230920f, 0.139173100960065690f,
  0.121869343405147490f, 0.104528463267653460f,
  0.087155742747658138f, 0.069756473744125455f, 0.052335956242943966f,
  0.034899496702501080f, 0.017452406437283376f, 0.000000000000000061f,
  -0.017452406437283477f, -0.034899496702500733f,
  -0.052335956242943620f, -0.069756473744125330f, -0.087155742747658235f,
  -0.104528463267653330f, -0.121869343405147370f, -0.139173100960065350f,
  -0.156434465040231040f, -0.173648177666930300f,
  -0.190808995376544800f, -0.207911690817759120f, -0.224951054343864810f,
  -0.241921895599667790f, -0.258819045102520850f, -0.275637355816999050f,
  -0.292371704722736660f, -0.309016994374947340f,
  -0.325568154457156420f, -0.342020143325668710f, -0.358367949545300270f,
  -0.374606593415912070f, -0.390731128489273600f, -0.406736643075800100f,
  -0.422618261740699330f, -0.438371146789077510f,
  -0.453990499739546750f, -0.469471562785890530f, -0.484809620246337000f,
  -0.499999999999999780f, -0.515038074910054270f, -0.529919264233204790f,
  -0.544639035015027080f, -0.559192903470746680f,
  -0.573576436351045830f, -0.587785252292473030f, -0.601815023152048380f,
  -0.615661475325658290f, -0.629320391049837280f, -0.642787609686539360f,
  -0.656059028990507500f, -0.669130606358858240f,
  -0.681998360062498370f, -0.694658370458997030f, -0.707106781186547460f,
  -0.719339800338651300f, -0.731353701619170460f, -0.743144825477394130f,
  -0.754709580222772010f, -0.766044443118977900f,
  -0.777145961456970680f, -0.788010753606721900f, -0.798635510047292940f,
  -0.809016994374947340f, -0.819152044288991580f, -0.829037572555041620f,
  -0.838670567945424160f, -0.848048096156425960f,
  -0.857167300702112220f, -0.866025403784438710f, -0.874619707139395740f,
  -0.882947592858926770f, -0.891006524188367790f, -0.898794046299167040f,
  -0.906307787036649940f, -0.913545457642600760f,
  -0.920504853452440150f, -0.927183854566787310f, -0.933580426497201740f,
  -0.939692620785908320f, -0.945518575599316740f, -0.951056516295153530f,
  -0.956304755963035440f, -0.961261695938318670f,
  -0.965925826289068200f, -0.970295726275996470f, -0.974370064785235250f,
  -0.978147600733805690f, -0.981627183447663980f, -0.984807753012208020f,
  -0.987688340595137660f, -0.990268068741570250f,
  -0.992546151641321980f, -0.994521895368273290f, -0.996194698091745550f,
  -0.997564050259824200f, -0.998629534754573830f, -0.999390827019095760f,
  -0.999847695156391270f, -1.000000000000000000f
};
