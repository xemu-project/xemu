#version 330
uniform sampler2D tex;
uniform vec4 in_ColorPrimary;
uniform vec4 in_ColorFill;
uniform float scale;
uniform float iTime;
out vec4 out_Color;

float pxRange           = 6.0;
vec4 fgColor            = in_ColorPrimary;
vec4 bgColor            = in_ColorFill;
const vec4 textPos      = vec4(0.01, 0, 0.98, 0.125);
const float pi          = 3.14159265359;
const float lineWidth   = 0.175;
const float duration    = 1.25;
const float pause       = 6.;
const int numParticles  = 35;
const int numSpotlights = 5;

// Thanks to: https://www.shadertoy.com/view/Xl2SRR
float random(float co)
{
    return fract(abs(sin(co*12.989)) * 43758.545);
}

float median(float r, float g, float b)
{
    return max(min(r, g), min(max(r, g), b));
}

// Thanks to: https://www.iquilezles.org/www/articles/functions/functions.htm
float quaImpulse( float k, float x )
{
    return 2.0*sqrt(k)*x/(1.0+k*x*x);
}

float getCurrentTime()
{
    return mod(iTime, duration+pause)/duration;
}

float getBox(vec2 uv, float x, float width)
{
    float lhs = sign(clamp(x - uv.x,         0., 1.));
    float rhs = sign(clamp(x - uv.x + width, 0., 1.));
    return rhs-lhs;
}

float getSweepingLinePos()
{
    return getCurrentTime()-lineWidth+textPos.x;
}

float getSweepingLine(vec2 uv)
{
    return getBox(uv, getSweepingLinePos(), lineWidth);
}

float getGradients(vec2 uv)
{
    float t = getCurrentTime();
    float l_s = abs(cos(t*pi*2.));
    float pos = t - uv.x + textPos.x;
    float left = l_s*smoothstep(0., 1., 0.5-abs(pos - lineWidth)*(20.+80.*(1.-l_s)));
    float r_s = abs(sin(t*pi*2.));
    float right = r_s*smoothstep(0., 1., 0.5-abs(pos)*(20.+80.*(1.-r_s)));
    float gradient_y = smoothstep(0.55, 1., 1.-abs(0.5-uv.y));
    return (left + right) * gradient_y;
}

vec2 getSpotlightPos(int i)
{
    float t = getCurrentTime();
    vec2 initialPos = textPos.zw*vec2(
        float(i)/float(numSpotlights-1), // Even
        sign(random(float(i+62)) - 0.6)*2.); // Top biased

    vec2 velocity;
    velocity.x = sign(random(float(i+63)) - 0.5)*0.7*(0.3+0.6*random(float(i+100)));
    velocity.y = -sign(initialPos.y)*0.8*(0.1+0.9*random(float(i+62)));
    return initialPos + velocity * t + vec2(textPos.x, 0.5); // Offset to center
}

float getSpotlights(vec2 uv)
{
    float t = getCurrentTime();
    float right = smoothstep(0.3, 0.7, 0.8-8.*abs(t - uv.x + textPos.x + 0.05));

    // Compute contribution from all spotlights to this frag
    float c = 0.;
    for (int j = 0; j < numSpotlights; j++) {
        vec2 pos = getSpotlightPos(j);
        float d = distance(uv, pos);
        c += (1.-smoothstep(0.04, 0.07835, d));
    }

    return 0.6*right + 0.4*c;
}

// Note: Does not include offset, added in getParticlePosition
vec2 getParticleInitialPosition(int i)
{
    return textPos.zw*vec2(
        float(i)/float(numParticles-1), // Even
        sign(random(float(i)) - 0.2)); // Top biased
}

float prob(float p, int i)
{
    return sign(clamp(random(float(i*30))-(1.-p), 0., 1.));
}

float getParticleLifespan(int i)
{
    return 1. + 1.25*exp(-10.*random(float(i*30))) + 0.5*prob(0.3, i);
}

float getParticleTime(int i)
{
    return getCurrentTime()-getParticleInitialPosition(i).x;
}

float getParticleAlive(int i)
{
  return clamp(sign(getParticleTime(i)), 0., 1.);
}

float getParticleIntensity(int i)
{
    return getParticleAlive(i)*clamp(getParticleLifespan(i)-getParticleTime(i), 0., 1.);
}

vec2 getParticlePosition(int i)
{
    float pt = getParticleTime(i);
    float impulse = quaImpulse(20., pt*0.25+0.05+0.4*random(float(i+30)));
    vec2 initialPos = getParticleInitialPosition(i);
    vec2 velocity;
    // Move mostly right, sometimes left
    velocity.x = 0.4*impulse*sign(random(float(i+66)) - 0.1)*(0.3 + 0.6*random(float(i + 100)));
    // Move vertically in whatever direction particle spawned in
    velocity.y = 0.8*impulse*sign(initialPos.y)*(0.1 + 0.9*random(float(i + 62)));
    return initialPos + getParticleAlive(i) * velocity * pt + vec2(textPos.x, 0.5); // Offset to center
}

float getParticles(vec2 uv)
{
    // Compute contribution from all particles to this frag
    float c = 0.;
    for (int j = 0; j < numParticles; j++) {
        vec2 pos = getParticlePosition(j);
        float d = distance(uv, pos);
        c += (1.-smoothstep(0.004, 0.00835,d))*getParticleIntensity(j);
    }

    return c;
}

void main()
{
    vec2 uv = gl_FragCoord.xy/vec2(512);
    float scale = 1.4;
    uv -= 0.5 * (1.-1./scale);
    uv *= scale;
    vec2 pos = uv;

    vec3 msd = texture(tex, vec2(pos.x, pos.y)).rgb;
    float sd = median(msd.r, msd.g, msd.b);
    float screenPxDistance = pxRange*(sd - 0.5);
    float opacity = clamp(screenPxDistance + 0.5, 0.0, 1.0);
    vec4 fill_color = mix(bgColor, fgColor, opacity);
    float outline = clamp(screenPxDistance + 1.6, 0., 1.);
    outline -= clamp(screenPxDistance - 1.6, 0., 1.);
    outline = smoothstep(0.5, 1., outline);

    vec4 line_color = mix(bgColor, fgColor, outline);
    out_Color = mix(fill_color, line_color, getSweepingLine(uv));
    float mask_rhs = clamp(sign(uv.x-lineWidth-getSweepingLinePos()),0.,1.);
    out_Color += fill_color*mask_rhs*getSpotlights(uv);
    out_Color += mix(vec4(0), fgColor, getParticles(uv));
    out_Color += 2.*fgColor*getBox(uv, textPos.x, textPos.z)*getGradients(uv);
}
