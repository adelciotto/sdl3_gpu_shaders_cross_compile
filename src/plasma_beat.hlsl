cbuffer Uniform_Block : register(b0, space3) {
  float  time : packoffset(c0);
  float2 resolution : packoffset(c0.y);
};

static const float PULSE_DURATION = 1.5;
static const float PI             = 3.1415926535897932384626433832795;
static const float TWO_PI         = PI * 2.0;
static const float PI_OVER_6      = PI / 6.0;

float mod(float x, float y) {
  return x - y * floor(x / y);
}

float3 mod(float3 x, float y) {
  return x - y * floor(x / y);
}

// Function from Iñigo Quiles (no cubic smoothing)
// https://www.shadertoy.com/view/MsS3Wc
float3 hsv_to_rgb(float3 c) {
  float3 rgb = clamp(abs(mod(c.x * 6.0 + float3(0.0, 4.0, 2.0), 6.0) - 3.0) - 1.0, 0.0, 1.0);
  return c.z * lerp(float3(1.0, 1.0, 1.0), rgb, c.y);
}

float2 rotate(float2 p, float angle) {
  return float2(cos(angle) * p.x + sin(angle) * p.y, -sin(angle) * p.x + cos(angle) * p.y);
}

float heart(float2 p, float2 center, float size, float angle) {
  float2 o  = (p - center) / (1.6 * size);
  float2 ro = rotate(o, angle);
  float  a  = ro.x * ro.x + ro.y * ro.y - 0.3;

  return step(a * a * a * 2.0, ro.x * ro.x * ro.y * ro.y * ro.y);
}

// Modified plasma effect from https://www.bidouille.org/prog/plasma
float3 plasma(float2 p, float scale) {
  float  angle = time * 0.3;
  float2 rp    = rotate(p, angle);
  rp *= scale;

  float v1 = sin(rp.x + time);
  float v2 = sin(rp.y + time);
  float v3 = sin(rp.x + rp.y + time);
  float v4 = sin(length(rp) + 1.7 * time);
  float v  = v1 + v2 + v3 + v4;

  v *= 2.0;
  float3 color = float3(1.0, 0.3 - sin(v + PI * .5) * 0.2, 0.8 - sin(v + PI * .5) * 0.2);
  return color * 0.5 + 0.5;
}

float4 main(float2 tex_coord : TEXCOORD0) : SV_Target {
  float2 frag_coord = tex_coord * resolution;
  float2 uv         = (2.0 * float2(frag_coord - 0.5 * resolution)) / resolution.y;

  float pulse_time = mod(time, PULSE_DURATION) / PULSE_DURATION;
  float pulse_beat = pow(pulse_time, 0.2) * 0.5 + 0.5;

  float pulse =
      1.0 + pulse_beat * 0.5 * sin(pulse_time * TWO_PI * 3.0 + uv.y * 0.5) * exp(-pulse_time * 4.0);
  float3 color = plasma(uv, pulse * 8.0);

  // Centre heart.
  float  radius      = pulse * 0.4;
  float  d           = heart(uv, float2(0, -0.07), radius, 0.0);
  float3 heart_color = lerp(float3(1.0, 1.0, 1.0), float3(0.95, 0.37, 0.47), pulse);
  color              = lerp(color, heart_color, d);

  // Rotating heart ring.
  pulse = 0.4 +
          pulse_beat * 0.3 * sin(pulse_time * TWO_PI * 3.0 + uv.x * 0.6) * exp(-pulse_time * 3.33);
  float ring_radius = 0.25 + pulse;
  for (float angle = PI_OVER_6; angle <= TWO_PI; angle += PI_OVER_6) {
    float  current_angle = time * 0.8 + angle;
    float2 center      = float2(ring_radius * cos(current_angle), ring_radius * sin(current_angle));
    float  d           = heart(uv, center, 0.08, current_angle);
    float3 heart_color = hsv_to_rgb(float3((current_angle / TWO_PI) + 0.5, 1.0, 1.0));
    color              = lerp(color, heart_color, d);
  }

  color = pow(color, float3(2.2, 2.2, 2.2));

  return float4(color, 1.0);
}
