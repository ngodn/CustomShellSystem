// Genessa eye and smoke material response, before engine lighting/fog output.
// Reconstructed from the shipped SM6 shaders; see genessa-doubles-materials.md.
float CSSGenessaPower(float value, float exponent)
{
    return value <= 2.980232949312267e-8f ? 0.0f : exp2(log2(value) * exponent);
}

float4 CSSGenessaEyeResponse(float facing, float power, float red, float green,
    float blue, float intensity, float adaptation)
{
    float falloff = CSSGenessaPower(facing, power);
    float r = red * falloff;
    float g = green * falloff;
    float b = blue * falloff;
    float luminance = r * 0.33000001311302185f + g * 0.5f + b * 0.1599999964237213f;
    float denominator = adaptation * (101.0f - 100.0f * CSSGenessaPower(luminance, 0.44999998807907104f));
    return float4(max(r * 100.0f * intensity / denominator, 0.0f),
                  max(g * 100.0f * intensity / denominator, 0.0f),
                  max(b * 100.0f * intensity / denominator, 0.0f), 1.0f);
}

float4 CSSGenessaSmokeResponse(float noise0, float noise1, float noise2,
    float uvY, float vertexRed, float facing, float maskPower, float opacity,
    float red, float green, float blue, float intensity, float adaptation)
{
    float grey = noise0 * 0.2126390039920807f + noise1 * 0.7151686549186707f
        + noise2 * 0.07219231873750687f;
    float exponent = 0.75f - uvY * 0.6000000238418579f;
    float x = CSSGenessaPower(noise0 + (grey - noise0) * 0.44999998807907104f, exponent) * (uvY * uvY);
    float y = CSSGenessaPower(noise1 + (grey - noise1) * 0.44999998807907104f, exponent) * (uvY * uvY);
    float z = CSSGenessaPower(noise2 + (grey - noise2) * 0.44999998807907104f, exponent) * (uvY * uvY);
    float r = red * x;
    float g = y * green;
    float b = z * blue;
    float luminance = r * 0.33000001311302185f + g * 0.5f + b * 0.1599999964237213f;
    float denominator = adaptation * (101.0f - 100.0f * CSSGenessaPower(luminance, 0.6499999761581421f));
    float alpha = saturate((facing * facing) * (CSSGenessaPower(vertexRed, maskPower) * sqrt(x)) * opacity);
    return float4(r * 100.0f * intensity / denominator,
                  g * 100.0f * intensity / denominator,
                  b * 100.0f * intensity / denominator, alpha);
}

#ifndef CSS_GENESSA_RESPONSE_ONLY
float2 CSSGenessaSmokeUV(float2 uv, float vertexGreen, float tiling, float time)
{
    float phase = fmod(time * 0.0016666667070239782f, 1.0f);
    float offset = vertexGreen * 0.6660000085830688f;
    return float2(uv.x * tiling + offset, (uv.y + phase * 510.0f) * tiling + offset);
}
#endif
