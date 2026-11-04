#include "StringEngine.h"
#include <algorithm>
#include <cmath>

namespace pmg
{
namespace
{
constexpr float pi = 3.14159265358979323846f;
template <typename T> T limit(T lo, T hi, T v) noexcept { return std::max(lo, std::min(hi, v)); }
float smoothstep(float x) noexcept { x = limit(0.f, 1.f, x); return x*x*(3.f-2.f*x); }
float position(float x) noexcept { return limit(.02f, .48f, x); }
}
float StringEngine::Waveguide::read(float delay) const noexcept
{
    delay = limit(1.f, float(delayBufferSize-3), delay);
    const int i = int(delay);
    const float f = delay - float(i);
    const auto a = history[size_t((index-i) & delayBufferMask)];
    const auto b = history[size_t((index-i-1) & delayBufferMask)];
    return a + f*(b-a);
}
void StringEngine::Waveguide::clear() noexcept
{
    history.fill(0.f); index=0; previous=dispersionState=previousPickup=0;
    spectralState.fill(0.f);
}
float StringEngine::Waveguide::propagate(float delay, float gain, float loss,
                                         float dispersion, float injected,
                                         const std::array<float,4>& bandLoss,
                                         const std::array<float,3>& spectralAlpha) noexcept
{
    const float returned = read(delay);
    const float filtered = (1.f-loss)*returned + loss*previous;
    previous = returned;
    // Stable unit-magnitude first-order allpass: dispersion, not artificial detuning.
    const float dispersed = dispersion*filtered + dispersionState;
    dispersionState = filtered - dispersion*dispersed;
    float physicallyDamped=dispersed;
    // Passivity-preserving mixture: monotone damping across frequency bands
    // means every coefficient below is nonnegative and sums to <= 1.
    const float b0=limit(0.f,.22f,bandLoss[0]);
    const float b1=limit(b0,.26f,bandLoss[1]);
    const float b2=limit(b1,.30f,bandLoss[2]);
    const float b3=limit(b2,.34f,bandLoss[3]);
    if(b3>0.f)
    {
        for(int k=0;k<3;++k)
            spectralState[size_t(k)] += spectralAlpha[size_t(k)]*(dispersed-spectralState[size_t(k)]);
        physicallyDamped=(b1-b0)*spectralState[0]
                        +(b2-b1)*spectralState[1]
                        +(b3-b2)*spectralState[2]+(1.f-b3)*dispersed;
    }
    history[size_t(index)] = limit(-2.f, 2.f, gain*physicallyDamped + injected);
    index = (index+1) & delayBufferMask;
    return returned;
}
void StringEngine::Waveguide::scatter(float location, float force, float delay) noexcept
{
    // Localized, balanced scattering into the two counter-propagating wavefronts.
    // This is a low-order approximation to a displacement-wave contact junction.
    const int a = limit(2, delayBufferSize-4, int(delay * position(location)));
    const int b = limit(2, delayBufferSize-4, int(delay * (1.f-position(location))));
    auto& va = history[size_t((index-a) & delayBufferMask)];
    auto& vb = history[size_t((index-b) & delayBufferMask)];
    va = limit(-2.f, 2.f, va + force*.5f);
    vb = limit(-2.f, 2.f, vb - force*.5f);
}
void StringEngine::String::start(int n, int stringNo, int fretNo, float velocity,
                                 float sr, Model m, const Controls& c) noexcept
{
    for (auto& w : transverse) w.clear();
    midiNote=n; stringNumber=stringNo; fret=fretNo;
    active=true; releasing=false; noteAge=0; rms=0;
    noteVelocity=limit(0.f, 1.f, velocity);
    const float frequency = 440.f*std::pow(2.f, float(n-69)/12.f);
    baseFrequency = frequency;
    targetDelay=limit(12.f, float(delayBufferSize-16), sr/frequency - 1.35f);
    desiredDelay=targetDelay;
    cachedBend=1000.f; cachedDecay=-1.f; cachedCharacter=-1.f;
    dispersion = m==Model::nylon ? .78f : m==Model::steel ? .48f : .28f;
    dispersion -= .10f*(c.material-.5f);
    dispersion=limit(.1f,.89f,dispersion);
    // Slightly different loss constants and detuning in the two physical planes
    // cause beating and an uneven, but deterministic, long-term tail.
    planeDetune = .00035f + .0014f*c.material;
    strainEnergy = 0.f;
    waveExchange = 0.f;
    if (m == Model::electric) {
        pickStiffness = 1.18f; strainSensitivity = .0035f;
        transverseLossRatio = 1.30f; polarizationExchange = .00016f;
        attackTimeScale = .74f;
    } else if (m == Model::steel) {
        pickStiffness = 1.04f; strainSensitivity = .0052f;
        transverseLossRatio = 1.39f; polarizationExchange = .00031f;
        attackTimeScale = .88f;
    } else if (m == Model::nylon) {
        pickStiffness = .62f; strainSensitivity = .0022f;
        transverseLossRatio = 1.52f; polarizationExchange = .00045f;
        attackTimeScale = 1.43f;
    } else {
        pickStiffness = 1.f; strainSensitivity = .0028f;
        transverseLossRatio = 1.28f; polarizationExchange = .00024f;
        attackTimeScale = 1.f;
    }
    const auto idx=size_t(limit(0,5,stringNumber));
    pickStiffness *= limit(.4f,2.5f,c.calibration.contactStiffness[idx]);
    perBandLoss = c.calibration.spectralLoss[idx];
    // Frequency-relative loss bands: fundamental, low, upper, very high partials.
    // The cutoff is calculated per physically assigned string and fret.
    for(int k=0;k<3;++k)
    {
        const float ratio=k==0?2.3f:k==1?4.8f:8.7f;
        const float fc=limit(80.f,sr*.40f,frequency*ratio);
        perBandAlpha[size_t(k)]=1.f-std::exp(-2.f*pi*fc/sr);
    }
    // Pick deflection follows velocity nonlinearly; force is released by a slip event.
    pickDepth = (.035f + .13f*noteVelocity*noteVelocity)*(m==Model::nylon ? .74f:1.f);
    pickSlip = .52f + .36f*c.hardness;
    contactLength = limit(12, int(sr*.005f), int(sr*(.00065f + .0028f*(1.f-c.hardness))*attackTimeScale));
    prevPickDisplacement=lastContact=fretDisplacement=previousBridge=0;
    lastMagnetic=pickupPrevious=circuitLo=circuitMid=rolloffState=0;
}
StringEngine::String::Output StringEngine::String::tick(
    float sr, Model m, const Controls& c, float bridgeReaction,
    float pitchBend, int& contactCounter) noexcept
{
    Output out;
    // Even an unfretted/unplayed string remains a resonating physical string.
    // The shared bridge can excite it sympathetically.
    if (cachedBend != pitchBend)
    {
        desiredDelay = limit(12.f, float(delayBufferSize-16),
            sr/(baseFrequency*std::pow(2.f,pitchBend/12.f)) - 1.35f);
        cachedBend = pitchBend;
    }
    if (cachedDecay != c.decay)
    {
        loopGain = std::pow(.001f,1.f/(baseFrequency*std::max(.4f,c.decay)));
        cachedDecay = c.decay;
    }
    if (cachedCharacter != c.character)
    {
        const float cutoff = limit(900.f,8500.f,6700.f-3500.f*c.character);
        circuitAlpha = 1.f-std::exp(-2.f*pi*cutoff/sr);
        cachedCharacter = c.character;
    }
    targetDelay += .0008f*(desiredDelay-targetDelay); // continuous string-length glide
    // Displacement increases string tension. The ensuing sharpened attack relaxes
    // as the vibration decays, without a synthesised pitch envelope or LFO.
    // Its small amount is model dependent and bounded to avoid runaway feedback.
    const float strain = std::min(.0045f, strainSensitivity * strainEnergy);
    const float d0 = targetDelay/(1.f+strain);
    const float d1 = d0*(1.f+planeDetune);
    const float x = position(c.pluck);
    const float contactPos = .025f + .11f*(float(fret)/24.f);
    const float pickSample = .5f*(transverse[0].read(d0*x) + transverse[0].read(d0*(1.f-x)));
    const float pickVelocity = pickSample-prevPickDisplacement;
    prevPickDisplacement=pickSample;

    // 1: Compliant pick (spring-damper) deflects the string, then slips free.
    float pickForce=0;
    if (active && noteAge < contactLength)
    {
        const float t = float(noteAge)/float(contactLength);
        const float press = smoothstep(t/.45f);
        const float withdrawal = 1.f-smoothstep((t-pickSlip)/(1.f-pickSlip));
        const float pickTip = pickDepth * press * withdrawal;
        const float penetration=std::max(0.f,pickTip-pickSample);
        // Discrete compliant force with damping: no uncontrolled impulse injection.
        pickForce=limit(0.f,.018f, .065f*pickStiffness*penetration
                       - .012f*std::sqrt(pickStiffness)*pickVelocity);
        if (t>pickSlip) pickForce *= 1.f-smoothstep((t-pickSlip)/(1.f-pickSlip));
    }
    // Two-polarization injection: pick angle changes modal energy ratio, not a static EQ.
    const float angle = limit(0.f,1.f,c.pickAngle);
    const float f0=45.f*pickForce*(.30f+.70f*angle);
    const float f1=45.f*pickForce*(1.f-.52f*angle);
    transverse[0].scatter(x,f0,d0);
    transverse[1].scatter(x,f1*.78f,d1);

    // 4: Fret/finger one-sided contact. Rattling is caused by penetrating the
    // clearance threshold and an opposing spring-damper reaction, not noise samples.
    float collision=0;
    const float vAtFret = .5f*(transverse[0].read(d0*contactPos) +
                                 transverse[0].read(d0*(1.f-contactPos)));
    // Material/touch influence action; realistic contact requires adjustable gap.
    const float clearance = .001f + .14f*limit(0.f,1.f,c.fretClearance)
                            + .020f*limit(0.f,1.f,c.touch);
    if (vAtFret < -clearance)
    {
        const float penetration = -clearance-vAtFret;
        const float speed=vAtFret-fretDisplacement;
        collision=limit(0.f,.010f, .21f*penetration - .09f*std::min(speed,0.f));
        transverse[0].scatter(contactPos,collision,d0);
        transverse[1].scatter(contactPos,collision*.22f,d1);
        ++contactCounter;
    }
    fretDisplacement=vAtFret;
    // A stopped finger dissipates transverse motion; muting is velocity-dependent.
    const float damping = releasing ? .91f - .08f*c.touch : 1.f;
    const float gainPerRoundTrip = loopGain;
    // Unequal energy losses in orthogonal displacement planes have a measurable
    // effect on the changing harmonic spectrum during decay.
    const float materialLoss = (m==Model::nylon ? .18f : .06f)*(1.f-c.material);
    const float loss = limit(.014f,.37f,(1.f-c.brightness)*.16f + materialLoss + (m==Model::nylon?.09f:.015f));
    const float feedback = limit(-.007f,.007f,bridgeReaction)*(c.coupling*.11f + .012f);
    // Conservative passive polarization mixing: the two channels exchange motion
    // with opposite sign. A very small coefficient avoids synthetic chorus beating.
    const float w0 = transverse[0].read(d0*.26f);
    const float w1 = transverse[1].read(d1*.26f);
    const float exchange = polarizationExchange*(w1-w0);
    waveExchange += .013f*(exchange-waveExchange);
    const float wav0 = transverse[0].propagate(d0,gainPerRoundTrip*damping,loss,dispersion,feedback + waveExchange,
                            perBandLoss,perBandAlpha);
    const float wav1 = transverse[1].propagate(d1,gainPerRoundTrip*damping,
                         limit(.014f,.42f,loss*transverseLossRatio),limit(.1f,.9f,dispersion+.14f),
                         feedback*.62f - waveExchange,perBandLoss,perBandAlpha);
    const float bridge = .62f*wav0 + .38f*wav1;
    // Strain energy is a low-pass of the string displacement in both planes;
    // tension stiffening is not driven by a synthetic noise or randomness source.
    const float excursion = std::min(1.f, wav0*wav0 + wav1*wav1);
    const float strainAlpha = std::min(.035f, 120.f/sr);
    strainEnergy += strainAlpha*(excursion-strainEnergy);
    out.bridge = bridge - previousBridge;
    previousBridge=bridge;
    out.fret=collision;
    // Radiated direct sound for acoustic styles: mixture of velocity and curvature.
    out.direct=(.62f*wav0+.38f*wav1)*.27f + out.bridge*.66f;
    // 5: finite pickup aperture spatially averages vibration before differentiation.
    const float px=position(c.pickup + limit(-.08f,.08f,c.calibration.pickupOffset[size_t(stringNumber)]));
    const float aperture = .003f + c.pickupAperture*.074f;
    const auto tap = [&](float pos) noexcept
    {
        pos=position(pos);
        return .67f*transverse[0].read(d0*pos) + .33f*transverse[1].read(d1*pos);
    };
    const float spatial = .25f*tap(px-aperture*.5f)+.5f*tap(px)+.25f*tap(px+aperture*.5f);
    const float coil2 = tap(px + .023f+.035f*c.character);
    const float humbucker = (1.f-.8f*c.character)*spatial + .8f*c.character*(spatial+coil2)*.5f;
    // Inductive velocity transduction followed by a damped two-pole RLC-like load.
    const float induction = 55.f*(humbucker - pickupPrevious);
    pickupPrevious=humbucker;
    const float a=circuitAlpha;
    circuitLo += a*(induction-circuitLo);
    circuitMid += a*(circuitLo-circuitMid);
    out.electric = circuitLo*(.28f+.25f*c.character)+circuitMid*(.72f-.25f*c.character);
    // Denormal/very low energy cleanup after release; no allocations.
    rms += .0009f*(bridge*bridge-rms);
    ++noteAge;
    if (releasing && noteAge > int(sr*.035f) && rms < 1.e-11f)
    {
        active=false; releasing=false;
        // Keep the waveguides alive for sympathetic vibration via the bridge.
    }
    return out;
}
void StringEngine::Mode::configure(float sr,float f,float q,float response,bool reset) noexcept
{
    const float omega=2.f*pi*limit(30.f,sr*.40f,f)/sr;
    const float r=std::exp(-pi*f/(std::max(.6f,q)*sr));
    targetA1=2.f*r*std::cos(omega);
    targetA2=r*r;
    targetB=(1.f-r)*response;
    smoothing=1.f-std::exp(-1.f/(sr*.045f));
    if(reset){a1=targetA1;a2=targetA2;b=targetB;y1=y2=0;}
}
float StringEngine::Mode::tick(float force) noexcept
{
    a1+=smoothing*(targetA1-a1);
    a2+=smoothing*(targetA2-a2);
    b+=smoothing*(targetB-b);
    const float y=a1*y1-a2*y2+b*force;
    y2=y1; y1=std::isfinite(y)?limit(-10.f,10.f,y):0.f;
    return y1;
}
void StringEngine::Body::configure(float sr,Model model,float size,
                                   const PhysicalCalibration& calibration,bool reset) noexcept
{
    // A compact 10-mode mechanical admittance, not an FIR/recorded impulse response.
    constexpr float fbase[]{82,111,182,210,284,397,535,742,1020,1580};
    constexpr float strength[]{.33f,.44f,.45f,.32f,.25f,.17f,.13f,.09f,.06f,.035f};
    for (size_t i=0;i<modes.size();++i)
    {
        float f=fbase[i]*limit(.70f,1.40f,calibration.modeFrequency[i])/limit(.7f,1.4f,size);
        if(model==Model::electric) f*=1.35f;
        if(model==Model::nylon) f*=.93f;
        const float qBase = model==Model::electric ? 2.5f :
                            model==Model::nylon ? 5.7f :
                            model==Model::steel ? 8.1f : 7.0f;
        const float q=(qBase+float(i)*.48f)*limit(.4f,2.5f,calibration.modeQ[i]);
        modes[i].configure(sr,f,q,strength[i]*limit(.25f,2.f,calibration.modeGain[i]),reset);
    }
    if(reset)clear();
}
void StringEngine::Body::clear() noexcept
{
    feedback=lowpass=0; radiated={};
    for(auto& mode:modes) mode.y1=mode.y2=0;
}
void StringEngine::Body::tick(float force,float coupling,float mobility) noexcept
{
    float returnV=0, left=0, right=0;
    for(size_t i=0;i<modes.size();++i)
    {
        const float old=modes[i].y1;
        const float y=modes[i].tick(force);
        const float velocity=y-old;
        returnV+=(i<5?1.f:.4f)*velocity;
        if(i%2==0) left += velocity*(i<4?1.f:.55f);
        else right += velocity*(i<4?1.f:.55f);
    }
    // The coupling loop is deliberately bounded; its physical calibration is pending.
    const float amount=limit(0.f,1.f,coupling)*(.025f+.075f*limit(0.f,1.f,mobility));
    lowpass += .08f*(returnV-lowpass);
    feedback=limit(-.007f,.007f,45.f*amount*lowpass);
    radiated={left,right};
}
void StringEngine::prepare(double sr,Model m,const Controls& c) noexcept
{
    rate=limit(8000.f,192000.f,float(sr)); model=m; bend=0; contacts=0;
    for(size_t i=0;i<strings.size();++i)
    {
        strings[i]=String{};
        strings[i].start(tuning[i],int(i),0,0.f,rate,m,c);
        strings[i].active=false;
    }
    controls=c;
    acousticBody.configure(rate,m,c.size,c.calibration,true);
    gain=std::pow(10.f,c.output*.05f);
    smoothedOutput=gain;
    smoothedBody=c.body;
    smoothedCoupling=c.coupling;
    lastBodyMotion=0;
    smoothAlpha = 1.f-std::exp(-1.f/(rate*.008f));
}
void StringEngine::setControls(const Controls& c) noexcept
{
    bool bodyChanged=std::abs(c.size-controls.size)>.0005f;
    for(int i=0;i<10 && !bodyChanged;++i) {
        const auto j=size_t(i);
        bodyChanged = c.calibration.modeFrequency[j]!=controls.calibration.modeFrequency[j]
                   || c.calibration.modeQ[j]!=controls.calibration.modeQ[j]
                   || c.calibration.modeGain[j]!=controls.calibration.modeGain[j];
    }
    if(bodyChanged)
        acousticBody.configure(rate,model,c.size,c.calibration,false); // smooth coefficients, retain motion
    controls=c;
    gain=std::pow(10.f,limit(-36.f,12.f,c.output)*.05f);
}
void StringEngine::setPitchBend(float semitones) noexcept {bend=limit(-2.f,2.f,semitones);}
void StringEngine::noteOn(int note,float velocity) noexcept
{
    if(note < 40 || note > 88 || velocity<=0.f) return;
    // Real playable strings: find an unused string on which this note is frettable.
    int selected=-1;
    float best=1.e12f;
    for(int s=0;s<maxVoices;++s)
    {
        const int fret=note-tuning[size_t(s)];
        if(fret<0 || fret>24) continue;
        const float score=(strings[size_t(s)].active?200.f:0.f)+float(fret)*1.7f
            + (s==0?.3f:0.f) + (strings[size_t(s)].midiNote==note ? -12.f : 0.f);
        if(score<best) {best=score;selected=s;}
    }
    if(selected<0) return;
    // A new attack on a string interrupts its prior wave state, as on a guitar.
    strings[size_t(selected)].start(note,selected,note-tuning[size_t(selected)],velocity,rate,model,controls);
}
bool StringEngine::noteOnString(int note,float velocity,int physicalString) noexcept
{
    if(physicalString<0 || physicalString>=maxVoices || velocity<=0.f) return false;
    const int fret=note-tuning[size_t(physicalString)];
    if(fret<0 || fret>24) return false;
    strings[size_t(physicalString)].start(note,physicalString,fret,velocity,rate,model,controls);
    return true;
}
void StringEngine::noteOff(int note) noexcept
{
    for(auto& s:strings) if(s.active && s.midiNote==note) s.releasing=true;
}
void StringEngine::allNotesOff() noexcept
{
    for(auto& s:strings) if(s.active) s.releasing=true;
}
int StringEngine::assignedString(int note) const noexcept
{
    for(auto& s:strings) if(s.active && s.midiNote==note) return s.stringNumber;
    return -1;
}
float StringEngine::stringEnergy(int physicalString) const noexcept
{
    if(physicalString<0||physicalString>=maxVoices)return 0;
    return strings[size_t(physicalString)].rms;
}
int StringEngine::activeVoices() const noexcept
{
    int count=0;for(auto& s:strings) count+=s.active?1:0;return count;
}
StereoFrame StringEngine::nextFrame() noexcept
{
    const float alpha=smoothAlpha;
    smoothedOutput += alpha*(gain-smoothedOutput);
    smoothedBody += alpha*(controls.body-smoothedBody);
    smoothedCoupling += alpha*(controls.coupling-smoothedCoupling);
    float bridgeForce=0, electric=0, direct=0, fret=0;
    for(auto& string:strings)
    {
        auto out=string.tick(rate,model,controls,acousticBody.feedback,bend,contacts);
        bridgeForce += out.bridge;
        electric += out.electric;
        direct += out.direct;
        fret += out.fret;
    }
    // 3: every string drives the same 10-mode soundboard, whose mobility
    // reacts back on *every* active string on the next sample.
    acousticBody.tick(bridgeForce,smoothedCoupling,controls.bridgeMobility);
    lastBodyMotion=acousticBody.feedback;
    const float bodyL=acousticBody.radiated.left;
    const float bodyR=acousticBody.radiated.right;
    const bool isElectric=model==Model::electric;
    const float dry=isElectric?electric : (direct+fret*.16f)*2.4f;
    const float bodyScale=isElectric?.05f:8.8f;
    const float wet=limit(0.f,1.f,smoothedBody);
    StereoFrame s;
    // Linear below -2.5 dBFS: no amp, waveshaper or coloration on normal DI levels.
    // Only unexpectedly hot signals enter a safety limiter.
    const auto safety = [](float v) noexcept {
        if(!std::isfinite(v))return 0.f;
        const float a=std::abs(v);
        return a<=.75f?v:std::copysign(.75f+.25f*std::tanh((a-.75f)*4.f),v);
    };
    s.left=safety(smoothedOutput*(dry*(1.f-wet*.38f)+bodyL*bodyScale*wet));
    s.right=safety(smoothedOutput*(dry*(1.f-wet*.38f)+bodyR*bodyScale*wet));
    return s;
}
float StringEngine::nextSample() noexcept
{
    const auto stereo=nextFrame();
    return .5f*(stereo.left+stereo.right);
}
} // namespace pmg
