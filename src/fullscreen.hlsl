struct Output {
  float2 tex_coord : TEXCOORD0;
  float4 position : SV_position;
};

Output main(uint id : SV_VertexID) {
  Output output;
  output.tex_coord = float2(float((id << 1) & 2), float(id & 2));
  output.position =
      float4((output.tex_coord * float2(2.0f, -2.0f)) + float2(-1.0f, 1.0f), 0.0f, 1.0f);
  return output;
}
