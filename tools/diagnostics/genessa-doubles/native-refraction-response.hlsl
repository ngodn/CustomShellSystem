// M_refraction_01's material inputs, before native base-pass lighting and fog.
float4 CSSRefractionDirection(float x, float y, float z, float angleX,
    float angleY, float angleZ, float pivot)
{
    float ax = angleX * 6.2831854820251465f;
    float ay = angleY * 6.2831854820251465f;
    float az = angleZ * 6.2831854820251465f;
    float vx = x - pivot;
    float vy = y - pivot;
    float vz = z - pivot;
    float diagonal = pivot + vx + vz;
    // The shipped graph adds three rotation offsets. Its third axis is
    // (1,0,1), not the usual Z axis, and is not normalized in this shader.
    float rx = pivot + vx * cos(ay) + vz * sin(ay);
    float rz = pivot + vz * cos(ay) - vx * sin(ay);
    float sy = pivot + vy * cos(ax) - vz * sin(ax);
    float sz = pivot + vz * cos(ax) + vy * sin(ax);
    float tx = diagonal + (x - diagonal) * cos(az) - vy * sin(az);
    float ty = pivot + vy * cos(az) + (x - z) * sin(az);
    float tz = diagonal + (z - diagonal) * cos(az) + vy * sin(az);
    return float4(rx + tx - x, sy + ty - y, rz + sz + tz - 2.0f * z, 1.0f);
}

float CSSRefractionDelta(float facing, float ior)
{
    float grazing = max(abs(1.0f - max(0.0f, facing)), 0.0001f);
    float power = grazing <= 2.980232949312267e-8f ? 0.0f : exp2(log2(grazing) * 5.0f);
    return (0.04f + 0.96f * power) * (ior - 1.0f);
}
