// shader.slang
[shader("vertex")]
void vertMain(
    uint vertexID : SV_VertexID,
    out float4 outPosition : SV_Position,
    out float3 outColor : COLOR)
{
    
    float2 positions[3] = {
        float2(0.0, -0.5),
        float2(-0.5, 0.5),
        float2(0.5, 0.5)
    };

    float3 colors[3] = {
        float3(1.0, 0.0, 0.0),
        float3(0.0, 1.0, 0.0),
        float3(0.0, 0.0, 1.0)
    };

    outPosition = float4(positions[vertexID], 0.0, 1.0);
    outColor = colors[vertexID].rgb;
}

[shader("fragment")]
void fragMain(
    float3 inColor : COLOR,
    out float4 outFragColor : SV_Target)
{
    outFragColor = float4(inColor, 1.0);
}
