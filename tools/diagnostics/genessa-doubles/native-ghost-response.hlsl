// Diagnostic reconstruction from the shipped SM6 AstralCopy pixel shaders.
// See docs/development/genessa-doubles-shaders.md for provenance and limits.
float CSSAstralPower(float value, float exponent)
{
    return value <= 2.980232949312267e-8f ? 0.0f : exp2(log2(value) * exponent);
}

// noise is the blue-channel result of the native six-sample projection.
// radius is the saturated distance in normalized pre-skinned bounds.
float4 CSSAstralResponse(float noise, float normalDotCamera, float localY,
    float radius, float opacity, float pixelDepth, float depthFade,
    float eyeAdaptation, bool corrupted)
{
    float facing = abs(normalDotCamera);
    float edge = 1.0f - facing;
    float value = facing * CSSAstralPower(saturate(sqrt(noise)), 1.45f)
        + CSSAstralPower(edge, 2.2f) * 0.2f;
    float ramp = saturate(saturate(value * 2.007617235183716f)
        - saturate((value - 0.49810290336608887f) * 3.558958053588867f) * 0.9f
        + saturate((value - 0.7790840268135071f) * 7.829707622528076f) * 0.9f
        - saturate((value - 0.9068027138710022f) * 10.729926109313965f) * 0.9f);
    float intensity = edge * 1.5600000619888306f + 0.06499999761581421f;
    float red;
    float green;
    float blue;
    if (corrupted)
    {
        float low = saturate(ramp * 1.461545467376709f);
        float high = saturate((ramp - 0.6842072606086731f) * 3.2623791694641113f);
        red = intensity * low;
        green = intensity * (high * 0.11936055123806f + low * 0.03688944876194f);
        blue = green;
    }
    else
    {
        float low = saturate(ramp * 1.2261288166046143f);
        float high = saturate((ramp - 0.8155750036239624f) * 5.7091593742370605f);
        red = (edge * 0.39000001549720764f + 0.016249999403953552f) * (high + low);
        green = intensity * (high * 0.07173675298690796f + low * 0.7847893834114075f);
        blue = intensity * low;
    }

    // The native CPU preshader writes sqrt(saturate(o)) squared to this uniform.
    float rootOpacity = sqrt(saturate(opacity));
    float inverseOpacity = 1.0f - rootOpacity * rootOpacity;
    float lower = (max(inverseOpacity, 0.5f) - 0.5f) * 2.0f;
    float upper = min(inverseOpacity * 2.0f, 1.0f);
    float dissolve = saturate((radius - lower) / (upper - lower));
    float alpha = saturate(CSSAstralPower(facing, 1.45f) * 2.0f)
        * saturate(CSSAstralPower(localY, 1.75f) * 2.0f - noise);
    alpha = saturate(alpha * dissolve * saturate((pixelDepth - 24.0f) / 256.0f) * depthFade);
    return float4(red / eyeAdaptation, green / eyeAdaptation, blue / eyeAdaptation, alpha);
}

#ifndef CSS_ASTRAL_RESPONSE_ONLY
float CSSAstralNoise(Texture2D noiseTexture, SamplerState noiseSampler,
    float3 local01, float3 normalWS, float timeSeconds)
{
    float phase = fmod(timeSeconds * 0.0011111111380159855f, 1.0f);
    float3 p = float3(local01.x, local01.y, local01.z * 2.0f);
    float3 animated = p + float3(0.0f, phase * 225.0f, -phase * 90.0f);
    float weightX = saturate(abs(normalWS.x) * 3.0f - 1.0f);
    float weightZ = saturate(abs(normalWS.z) * 3.0f - 1.0f);
    float a = noiseTexture.Sample(noiseSampler, -animated.xz).r;
    float b = noiseTexture.Sample(noiseSampler, -animated.yz).r;
    float c = noiseTexture.Sample(noiseSampler, -animated.xy).r;
    float distortion = lerp(lerp(a, b, weightX), c, weightZ) * 0.5f;
    float3 distorted = (p + distortion) * -2.0f;
    a = noiseTexture.Sample(noiseSampler, distorted.xz).b;
    b = noiseTexture.Sample(noiseSampler, distorted.yz).b;
    c = noiseTexture.Sample(noiseSampler, distorted.xy).b;
    return lerp(lerp(a, b, weightX), c, weightZ);
}
#endif
