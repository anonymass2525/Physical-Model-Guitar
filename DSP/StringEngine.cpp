#include "StringEngine.h"
#include "MeasuredBridgeZenodo.h"
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
    // A fresh strike does not destroy the energy of an already vibrating string.
    // Retain a small fraction for a repeated note; if fret changes, the finger
    // damps the old string more strongly, as in a real repositioning gesture.
    // This operation happens at note-on (not in the per-sample tick loop).
    const bool reattack = c.physics07 && (active || rms > 1.e-10f) && noteAge > 12;
    const float retain = (midiNote == n ? .21f : .055f);
    if (reattack)
    {
        for (auto& w : transverse)
        {
            for (float& sample : w.history) sample *= retain;
            w.previous *= retain;
            w.dispersionState *= retain;
            for (float& v : w.spectralState) v *= retain;
        }
    }
    else for (auto& w : transverse) w.clear();
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
    longitudinalDisplacement=longitudinalVelocity=longitudinalBridge=0.f;
    tangentialDeflection=previousPickTip=0.f;
    fretContactHistory.fill(0.f);
    fretContactMemory=0.f;
    availableContactFrets=limit(0,5,24-fretNo);
    for(int k=0;k<availableContactFrets;++k)
    {
        const float relativeFret=float(k+1);
        fretLocation[size_t(k)]=position(1.f-std::pow(2.f,-relativeFret/12.f));
        fretHeightMultiplier[size_t(k)]=(.45f+.085f*relativeFret)
                                  *(1.f+.028f*std::sin(2.3f*float(k+fretNo)));
    }
    // Saturated reduced-order longitudinal resonance: from the open-string
    // longitudinal stiffness and vibrating length, with anti-alias headroom.
    // Frequencies are effective model values pending sensor calibration.
    const float lscale=m==Model::nylon?5.8f:m==Model::steel?9.2f:10.5f;
    longitudinalFrequency=limit(200.f,sr*.11f,frequency*lscale);
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
    prevPickDisplacement=lastContact=fretDisplacement=previousBridge=previousBridgeX=previousBridgeY=0;
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
    const float strain = c.physics06
        ? std::min(.006f, strainSensitivity*(.70f*strainEnergy +
                4.5f*std::abs(longitudinalDisplacement)))
        : std::min(.0045f, strainSensitivity * strainEnergy);
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
    if(c.physics06 && active && noteAge < contactLength)
    {
        // Spring and bounded Stribeck friction. The tangential compliance is
        // only active during pick/string contact, never an artificial LFO.
        const float tip=pickDepth*smoothstep(float(noteAge)/(contactLength*.44f));
        const float vtip=tip-previousPickTip;
        previousPickTip=tip;
        const float vrel=vtip-pickVelocity;
        const float vstribeck=.00024f + .00032f*(1.f-c.hardness);
        const float ratio=limit(-9.f,9.f,vrel/vstribeck);
        const float stribeck=std::exp(-ratio*ratio);
        const float friction=.50f + .32f*stribeck;
        const float deflectionDrive=limit(-.004f,.004f,(tip-pickSample)*.075f);
        tangentialDeflection += .06f*(deflectionDrive-tangentialDeflection);
        const float frictionImpulse=limit(-.0015f,.0015f,
            tangentialDeflection*friction*.12f + vrel*friction*.016f);
        // Smoothly lose contact: avoids a velocity step at pick release.
        const float release=1.f-smoothstep((float(noteAge)/contactLength-pickSlip)/
                                          std::max(.03f,1.f-pickSlip));
        transverse[0].scatter(x,frictionImpulse*release,d0);
        transverse[1].scatter(x,-.55f*frictionImpulse*release,d1);
    }
    transverse[0].scatter(x,f0,d0);
    transverse[1].scatter(x,f1*.78f,d1);

    // 4: Distributed fret/finger contact. In v0.6 a single contact junction
    // approximated all frets. v0.7 uses up to five actual downstream fret
    // coordinates, with geometric action and velocity-dependent restitution.
    // Contact is one-sided and bounded, active only if a wave crosses the
    // local clearance. There is no added white noise or synthetic buzz.
    float collision = 0.f;
    if (c.physics07)
    {
        const float baseClearance = .001f + .14f*limit(0.f,1.f,c.fretClearance)
                                           + .020f*limit(0.f,1.f,c.touch);
        const int fretsAvailable = availableContactFrets;
        float remainingBudget = .009f;  // upper bound on added scattering per frame
        for (int k=0;k<fretsAvailable;++k)
        {
            // Fret geometry is precomputed at note-on: no pow()/sin() in DSP tick.
            const float pos=fretLocation[size_t(k)];
            const float displacement=.5f*(transverse[0].read(d0*pos)
                    +transverse[0].read(d0*(1.f-pos)));
            // String height increases away from the stopping finger. Staggered
            // fret crown heights approximate realistic, imperfect geometry.
            const float height=fretHeightMultiplier[size_t(k)];
            const float clearance=baseClearance*height;
            const float velocity=displacement-fretContactHistory[size_t(k)];
            fretContactHistory[size_t(k)] = displacement;
            if(displacement < -clearance && remainingBudget > 0.f)
            {
                const float penetration = -clearance-displacement;
                // Compliant one-sided contact approximation;
                // the damping contribution is active only on approach.
                const float activation=smoothstep(penetration/(.009f+.075f*clearance));
                float reaction = activation*(.125f*penetration
                             + .055f*std::max(0.f,-velocity));
                reaction=limit(0.f,std::min(.003f,remainingBudget),reaction);
                remainingBudget -= reaction;
                transverse[0].scatter(pos,reaction,d0);
                transverse[1].scatter(pos,reaction*.17f,d1);
                collision+=reaction;
                ++contactCounter;
            }
        }
        // Persistent contact activity measures mechanical touch, but is not
        // transformed into independent noise or tremolo.
        fretContactMemory += .025f*(collision-fretContactMemory);
    }
    else
    {
        // v0.6 single-junction contact, bit-identical when v0.7 is disabled.
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
    }
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
    float axialVelocity=0.f;
    if(c.physics06)
    {
        // Reduced-order longitudinal resonator excited by transverse strain.
        // Semi-implicit stable damped recurrence (w <= .70 radians/sample).
        // Saturated stretch estimate is nonnegative, therefore the mean tension
        // increases with transverse displacement, not via arbitrary pitch LFO.
        const float w=limit(.0001f,.69f,2.f*pi*longitudinalFrequency/sr);
        const float zeta=m==Model::nylon?.10f:m==Model::steel?.055f:.085f;
        const float slope0=wav0 - w0;
        const float slope1=wav1 - w1;
        const float slopeEnergy=limit(0.f,.15f,2.2f*(slope0*slope0+slope1*slope1)
                                                    + .018f*strainEnergy);
        longitudinalVelocity += w*w*(slopeEnergy-longitudinalDisplacement)
                                  -2.f*zeta*w*longitudinalVelocity;
        longitudinalVelocity=limit(-.14f,.14f,longitudinalVelocity);
        longitudinalDisplacement=limit(-.14f,.14f,
                                     longitudinalDisplacement+longitudinalVelocity);
        axialVelocity=longitudinalVelocity;
    }
    out.bridge = bridge - previousBridge;
    if(c.physics06)
    {
        const float axialContribution=(m==Model::nylon?.075f:.115f)*axialVelocity;
        // Axial reaction reaches the shared bridge; no direct synthesized buzz.
        out.bridge += axialContribution;
        longitudinalBridge=axialContribution;
    }
    previousBridge=bridge;
    if(c.physics08 || c.physics09) {
        // Orthogonal string-end velocity proxies. The measured boundary converts
        // force to bridge velocity; these are normalized string-source proxies,
        // not an SI-calibrated string impedance measurement.
        out.bridgeX=wav0-previousBridgeX;
        out.bridgeY=wav1-previousBridgeY;
        previousBridgeX=wav0;previousBridgeY=wav1;
    }
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
    auto bridgeModes=(c.measuredBridge08 && m==Model::nylon)
        ? zenodoMeasuredBridgeCandidate() : MeasuredBridge::fallbackGuitar(int(m));
    for(size_t i=0;i<bridgeModes.size();++i) {
        bridgeModes[i].frequencyHz *= c.calibration.modeFrequency[i]/std::max(.7f,c.size);
        bridgeModes[i].Q *= c.calibration.modeQ[i];
        bridgeModes[i].effectiveMass /= std::max(.25f,c.calibration.modeGain[i]);
    }
    measuredBridge.prepare(rate,bridgeModes);
    twoPortBridge.prepare(rate,bridgeModes);
    powerStrings10.prepare(rate,int(model));
    powerStrings10.setDecay(c.decay);
    powerStrings10.setPhysics11(c.physics11 && c.physics10);
    powerStrings10.setContactParameters(c.hardness,c.fretClearance,c.pickup);
    stringPortZ09=TwoPortBridge::normalizedImpedance(int(model));
    bridgeReflections09.fill(0.f);
    bridgePortFeedback=0.f;
    gain=std::pow(10.f,c.output*.05f);
    smoothedOutput=gain;
    smoothedBody=c.body;
    smoothedCoupling=c.coupling;
    lastBodyMotion=0;
    smoothAlpha = 1.f-std::exp(-1.f/(rate*.008f));
}
void StringEngine::setControls(const Controls& c) noexcept
{
    bool bodyChanged=std::abs(c.size-controls.size)>.0005f ||
        (c.measuredBridge08 != controls.measuredBridge08);
    for(int i=0;i<10 && !bodyChanged;++i) {
        const auto j=size_t(i);
        bodyChanged = c.calibration.modeFrequency[j]!=controls.calibration.modeFrequency[j]
                   || c.calibration.modeQ[j]!=controls.calibration.modeQ[j]
                   || c.calibration.modeGain[j]!=controls.calibration.modeGain[j];
    }
    if(bodyChanged) {
        acousticBody.configure(rate,model,c.size,c.calibration,false); // legacy smooth coefficients
        if((controls.physics08 && c.physics08) || (controls.physics09 && c.physics09)) {
            // Parameter change updates a passive model. Live interpolation of
            // calibrated mechanical poles is a separate research requirement.
            auto modes=(c.measuredBridge08 && model==Model::nylon)
                ? zenodoMeasuredBridgeCandidate() : MeasuredBridge::fallbackGuitar(int(model));
            for(size_t i=0;i<modes.size();++i) {
                modes[i].frequencyHz*=c.calibration.modeFrequency[i]/std::max(.7f,c.size);
                modes[i].Q*=c.calibration.modeQ[i];
                modes[i].effectiveMass/=std::max(.25f,c.calibration.modeGain[i]);
            }
            measuredBridge.prepare(rate,modes);
            twoPortBridge.prepare(rate,modes);
            bridgeReflections09.fill(0.f);
        }
    }
    controls=c;
    powerStrings10.setDecay(c.decay);
    powerStrings10.setPhysics11(c.physics11 && c.physics10);
    powerStrings10.setContactParameters(c.hardness,c.fretClearance,c.pickup);
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
    powerStrings10.strike(selected,note,velocity,controls.pluck);
}
bool StringEngine::noteOnString(int note,float velocity,int physicalString) noexcept
{
    if(physicalString<0 || physicalString>=maxVoices || velocity<=0.f) return false;
    const int fret=note-tuning[size_t(physicalString)];
    if(fret<0 || fret>24) return false;
    strings[size_t(physicalString)].start(note,physicalString,fret,velocity,rate,model,controls);
    powerStrings10.strike(physicalString,note,velocity,controls.pluck);
    return true;
}
void StringEngine::noteOff(int note) noexcept
{
    for(auto& s:strings) if(s.active && s.midiNote==note) {s.releasing=true;powerStrings10.release(s.stringNumber);}
}
void StringEngine::allNotesOff() noexcept
{
    for(auto& s:strings) if(s.active) s.releasing=true;
    powerStrings10.releaseAll();
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
    float bridgeForceX=0, bridgeForceY=0;
    std::array<double,maxVoices> incidence09{};
    for(int stringIndex=0;stringIndex<maxVoices;++stringIndex)
    {
        auto& string=strings[size_t(stringIndex)];
        // The old waveguide is retained for A/B reproducibility. In 0.9 the
        // outgoing scattering correction returns per string, not one global
        // bridge-feedback value. This adapter is still signal-normalised.
        const float reaction = controls.physics09 ? bridgeReflections09[size_t(stringIndex)]
            : controls.physics08 ? bridgePortFeedback : acousticBody.feedback;
        auto out=string.tick(rate,model,controls,reaction,bend,contacts);
        if(controls.physics09)
            incidence09[size_t(stringIndex)]=.70*double(out.bridgeX)+.30*double(out.bridgeY);
        bridgeForce += out.bridge;
        bridgeForceX += out.bridgeX; bridgeForceY += out.bridgeY;
        electric += out.electric;
        direct += out.direct;
        fret += out.fret;
    }
    // The new fully accounted waveguide subsystem runs independently
    // from the legacy 0.9 dry/contact model for safe A/B comparison.
    // Its own 12 wave ports, losses, nut reflections, and bridge are in SI
    // units. Legacy 0.9 DSP is deliberately excluded from this energy ledger.
    const auto power10=powerStrings10.tick();
    // 3: every string drives the same 10-mode soundboard, whose mobility
    // reacts back on *every* active string on the next sample.
    float bodyL=0.f,bodyR=0.f;
    if(controls.physics09) {
        // Passive six-wave-port -> two-location spatial bridge scattering.
        // Each positive impedance and the port interpolation is used with
        // its exact transpose, so the boundary cannot create wave energy.
        const auto port=twoPortBridge.tick(incidence09,stringPortZ09);
        const float mobility=std::max(0.f,controls.bridgeMobility);
        for(int i=0;i<maxVoices;++i) {
            // b_i = v_i - a_i. The old delay-line already reflects its own
            // arriving displacement wave, therefore only the difference
            // versus a rigid termination (-a_i) is reinjected here. This is
            // NOT a proof of energy balance for the *legacy* delay line.
            const double endpoint=port.stringEndpointVelocity[size_t(i)];
            bridgeReflections09[size_t(i)]=float(std::clamp(-.20*endpoint*double(mobility),-.007,.007));
        }
        lastBodyMotion=.5f*(float(port.bridgeVelocity[0])+float(port.bridgeVelocity[1]));
        const float radiationGain=(model==Model::electric?4.f:2.f);
        bodyL=radiationGain*float(port.radiatedL);
        bodyR=radiationGain*float(port.radiatedR);
    }
    else if(controls.physics08) {
        // A common collocated two-axis modal admittance accepts forces from all
        // six strings. Its damped modes cannot self-excite in isolation.
        // The legacy displacement-waveguide contact here remains a bounded
        // APPROXIMATION, not an exactly energy-conserving scattering junction.
        // The measured mobility is in SI units, but the synthesized string's
        // contact force currently is not. This explicit scale is a research
        // normalisation, NOT a calibrated string force in newtons.
        const float proxyForceScale=(controls.measuredBridge08 && model==Model::nylon)
                                   ? 21.6f : 3600.f;
        const float mobilityGain=.45f+.55f*std::max(0.f,smoothedCoupling);
        const auto port=measuredBridge.tick(bridgeForceX*proxyForceScale*mobilityGain,
                                            bridgeForceY*proxyForceScale*mobilityGain);
        bridgePortFeedback=std::max(-.007f,std::min(.007f,
            -.12f*port.velocityX*std::max(0.f,controls.bridgeMobility)));
        lastBodyMotion=bridgePortFeedback;
        const float radiationGain=(model==Model::nylon?2.0f:model==Model::steel?2.0f:4.f); // normalized scale, not calibrated Pa per N.
        bodyL=radiationGain*port.radiatedLeft;
        bodyR=radiationGain*port.radiatedRight;
    }else{
        acousticBody.tick(bridgeForce,smoothedCoupling,controls.bridgeMobility);
        lastBodyMotion=acousticBody.feedback;
        bodyL=acousticBody.radiated.left;
        bodyR=acousticBody.radiated.right;
    }
    const bool isElectric=model==Model::electric;
    float dry=isElectric?electric : (direct+fret*.16f)*2.4f;
    if(controls.physics10) {
        // Research A/B layer: lossless power-wave core provides additional
        // string/bridge character, while the older nonlinear pick/fret model
        // remains as a separate *not-energy-accounted* dry renderer.
        const float physical=limit(-.3f,.3f,float(power10.stringVelocity));
        dry += physical*(isElectric?14.f:3.f);
        if(controls.physics11) {
            // Non-invasive pickup observation from the new velocity-wave state.
            // Adds physical pickup-position comb behaviour without claiming
            // to simulate a complete electromagnetic load.
            const float sensed=limit(-.25f,.25f,float(power10.pickupVelocity));
            dry+=sensed*(isElectric?4.0f:1.2f);
        }
        bodyL+=float(power10.left)*6.f;
        bodyR+=float(power10.right)*6.f;
    }
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
