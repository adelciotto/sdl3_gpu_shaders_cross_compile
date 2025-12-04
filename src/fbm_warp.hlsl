cbuffer Uniform_Block : register(b0, space3) {
  float  time : packoffset(c0);
  float2 resolution : packoffset(c0.y);
};

#define mod(x, y) (x - y * floor(x / y))

// https://www.shadertoy.com/view/4dS3Wd
// By Morgan McGuire @morgan3d, http://graphicscodex.com
float hash(float n) {
  return frac(sin(n) * 1e4);
}

float hash(float2 p) {
  return frac(1e4 * sin(17.0 * p.x + p.y * 0.1) * (0.1 + abs(sin(p.y * 13.0 + p.x))));
}

float noise(float2 x) {
  float2 i = floor(x);
  float2 f = frac(x);

  float a = hash(i);
  float b = hash(i + float2(1.0, 0.0));
  float c = hash(i + float2(0.0, 1.0));
  float d = hash(i + float2(1.0, 1.0));

  float2 u = f * f * (3.0 - 2.0 * f);
  return lerp(a, b, u.x) + (c - a) * u.y * (1.0 - u.x) + (d - b) * u.x * u.y;
}

float fbm(float2 x, float H) {
  float G = exp2(-H);
  float f = 1.0;
  float a = 1.0;
  float t = 0.0;
  for (int i = 0; i < 4; i++) {
    t += a * noise(f * x);
    f *= 2.0;
    a *= G;
  }

  return t;
}

float pattern(float2 p, out float2 q, out float2 r) {
  static const float2 FLOW_DIR = float2(1.0, 0.3);

  float  flow_speed = time * 0.16;
  float  pulse      = 1.0 + sin(time * 0.2) * 0.1;
  float2 pulsed     = p * pulse;

  q = float2(fbm(pulsed, 5.0), fbm(pulsed + float2(5.2, 1.3), 5.0));
  r = float2(
      fbm(p + 4.0 * q + float2(1.7, 9.2) + FLOW_DIR * flow_speed, 5.0),
      fbm(p + 4.0 * q + float2(8.3, 2.8) - FLOW_DIR * flow_speed * 0.7, 5));

  return fbm(p + 4.0 * r + time * 0.01, 7);
}

float4 main(float2 tex_coord : TEXCOORD0) : SV_Target {
  float2 frag_coord = tex_coord * resolution;
  float2 p          = float2(frag_coord - 0.5 * resolution);
  p                 = 4.0 * p / resolution.y;

  float2 scroll =
      float2(time * 0.01 + sin(time * 0.05) * 1.5, time * 0.025 + cos(time * 0.07) * 1.5);

  float2 q, r;
  float  f = pattern(p + scroll, q, r);

  float3 normal = normalize(float3((q.x - 0.5) * 2.0, (r.y - 0.5) * 2.0, 1.0));

  float q_len = length(q) * 0.5;
  float r_len = length(r) * 0.5;

  static const float3 col1 = float3(0.1, 0.0, 0.0);
  static const float3 col2 = float3(0.6, 0.1, 0.0);
  static const float3 col3 = float3(1.0, 0.4, 0.0);
  static const float3 col4 = float3(0.968, 0.965, 0.923);

  float3 color = lerp(col1, col2, smoothstep(0.0, 0.4, f));
  color        = lerp(color, col3, smoothstep(0.3, 0.8, f));
  color        = lerp(color, col4, smoothstep(0.8, 1.0, f));
  color        = lerp(color, color * 1.3, smoothstep(0.3, 0.8, q_len));
  color        = lerp(color, col2, smoothstep(0.6, 1.0, r_len) * 0.3);

  float3 light_dir = normalize(float3(0.5, 0.8, 1.2));
  float  diffuse   = max(dot(normal, light_dir), 0.0);
  float  lighting  = 0.33 + diffuse * 0.8;

  color *= lighting;
  color = pow(color, float3(2.2, 2.2, 2.2));

  static const float VIGNETTE_RADIUS   = 1.9;
  static const float VIGNETTE_SOFTNESS = 0.85;
  float2             uv                = (frag_coord - 0.5 * resolution) / (resolution.y * 0.5);
  float              dist              = length(uv);
  float vignette = smoothstep(VIGNETTE_RADIUS, VIGNETTE_RADIUS - VIGNETTE_SOFTNESS, dist);
  color *= vignette;

  return float4(color, 1.0);
}
