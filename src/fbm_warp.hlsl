cbuffer Uniform_Block : register(b0, space3) {
  float  time : packoffset(c0);
  float2 resolution : packoffset(c0.y);
};

float4 main(float2 tex_coord : TEXCOORD0) : SV_Target {
  float2 frag_coord = tex_coord * resolution;
  float2 uv         = (frag_coord - 0.5 * resolution) / resolution.y;

  float3 col = 0.5 + 0.5 * sin(time + uv.xyx + float3(0.0, 2.0, 4.0));
  return float4(col, 1.0);
}
