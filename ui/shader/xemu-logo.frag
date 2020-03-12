#version 330
#define msdf tex
uniform sampler2D tex;
uniform vec4 in_ColorPrimary;
uniform vec4 in_ColorSecondary;
uniform vec4 in_ColorFill;
uniform float scale;
uniform float iTime;
in  vec2 Texcoord;
out vec4 out_Color;

float pxRange          = 6.0;
vec4 bgColor           = in_ColorFill;
vec4 fgColor           = in_ColorPrimary;
vec4 particleColor     = in_ColorSecondary;
const int numParticles = 40;
const float duration   = 1.1;
const float pause      = 5.0;
const vec4 textPos     = vec4(0.01, 0, 0.98, 0.125);
const float lineWidth  = 0.15;

// Thanks to: https://www.shadertoy.com/view/Xl2SRR
float random(float co)
{
    return fract(sin(co*12.989) * 43758.545);
}

float median(float r, float g, float b) {
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
    float rhs = sign(clamp(x-uv.x+width, 0., 1.));
    float lhs = sign(clamp(x-uv.x,       0., 1.));
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
    float gw = lineWidth/2.;
    float left  = getBox(uv, getSweepingLinePos() - gw,        gw)*smoothstep(0., 1., (gw + lineWidth - (t - uv.x + textPos.x))/lineWidth);
    float right = getBox(uv, getSweepingLinePos() + lineWidth, gw)*smoothstep(0., 1., (gw             + (t - uv.x + textPos.x))/lineWidth);
    float gradient_y = smoothstep(0.8, 1., 1.-abs(0.5-uv.y));
    return (left + right) * gradient_y;
}

// Note: Does not include offset, added in getParticlePosition
vec2 getParticleInitialPosition(int i)
{
    vec2 pos;
    pos.x = float(i)/float(numParticles-1); // Even
    pos.y = sign(random(float(i)) - 0.1); // Top biased
    return pos*textPos.zw;
}

float getParticleTime(int i)
{
    // Compute based on initial x due to sweeping reveal
    return getCurrentTime()-getParticleInitialPosition(i).x;
}

float getParticleIntensity(int i)
{
    float lifespan = 1.0 + 0.4*(random(float(i*44))-0.5);
    float alive = clamp(sign(getParticleTime(i)), 0., 1.);
    return alive*clamp(lifespan-getParticleTime(i), 0., 1.);
}

vec2 getParticlePosition(int i)
{
    float pt = getParticleTime(i);
    float alive = clamp(sign(pt), 0., 1.);
    float falloff = 10.;
    float impulse = quaImpulse(falloff, pt+0.8)+0.2;
    vec2 pos = getParticleInitialPosition(i);

    vec2 velocity;
    // Move mostly right, but sometimes left
    velocity.x  = sign(random(float(i+32*3030))-0.2);
    velocity.x *= impulse*1.25*(0.00 + random(float(i+62)));
    // Move vertically in whatever direction we spawned in
    velocity.y  = sign(pos.y);
    velocity.y *= impulse*1.40*(0.05 + random(float(i+62)));
    return pos + alive * velocity * pt + vec2(textPos.x, 0.5); // Offset to center
}

float getParticles(vec2 uv)
{
    // Compute contribution from all particles to this frag
    float c = 0.;
    for (int j = 0; j < numParticles; j++) {
        vec2 pos = getParticlePosition(j);
        float d = distance(uv, pos);
        c += (1.-smoothstep(0.01, 0.01035,d))*getParticleIntensity(j);
    }

    return c;
}

void main()
{
    // Normalized pixel coordinates (from 0 to 1)
    vec2 uv = gl_FragCoord.xy/vec2(512);
    float scale = 1.4;

    // Center when scaling
    uv -= 0.5 * (1.-1./scale);
    uv *= scale;
    vec2 pos = uv;

    // Get signed distance from the input texture
    // Thanks to https://github.com/Chlumsky/msdfgen
    vec2 msdfUnit = pxRange/vec2(textureSize(msdf, 0));
    vec3 s = texture(msdf, pos).rgb;

    // Create an alpha mask for the text
    float sigDist = median(s.r, s.g, s.b) - 0.5;
    sigDist *= dot(msdfUnit, 0.5/fwidth(pos));

    float smoothing = 4.-scale;
    float fill = smoothstep(0.5 - smoothing, 0.5 + smoothing, sigDist);
    vec4 fill_color = mix(bgColor, fgColor, fill);

    float outline = smoothstep(0.65, 0.80, 1.0-abs(sigDist/15.0-(-0.3333*scale+0.6667)+0.05));
    vec4 line_color = mix(bgColor, fgColor, outline);

    out_Color = mix(fill_color, line_color, getSweepingLine(uv));
    out_Color += mix(bgColor, particleColor, getParticles(uv));
    out_Color += 2.*vec4(1.)*getBox(uv, textPos.x, textPos.z)*getGradients(uv);
}
