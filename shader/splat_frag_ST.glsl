/*
    Copyright (c) 2025 Shakiba Kheradmand
*/

/*%%HEADER%%*/

layout(location = 0) in vec4 frag_color;    // Radiance of the splat (passed from geometry shader)
layout(location = 1) in vec3 frag_cov2inv;  // Inverse of the 2D screen space covariance matrix
layout(location = 2) in vec2 frag_p;        // 2D screen space center of the Gaussian

uniform uint u_randomSeed;

out vec4 out_color;  // Final pixel color output

const float THRESHOLD = 1.0 / 255.0;


uint hash(uint x) {
    x ^= x >> 16u;
    x *= 0x7feb352du;
    x ^= x >> 15u;
    x *= 0x846ca68bu;
    x ^= x >> 16u;
    return x;
}


float randomUniform(uint seed_in) {
    // Ensure random value is de-correlated wrt screen space coordinates, MSAA
    // sample ID, and splat. Try to pack intergers for all these into a 32 bit
    // value, hash, then normalize to [0, 1].

    uint seed = uint(gl_FragCoord.x);
    seed ^= uint(gl_FragCoord.y) << 20u;
#ifdef GL_ARB_sample_shading
    seed ^= uint(gl_SampleID) << 8u;
#endif
    seed += uint(gl_PrimitiveID);
    seed += seed_in;
    return float(hash(seed)) / 4294967295.0;
}


void main() {
    const vec2 d = gl_FragCoord.xy - frag_p;  // Distance from Gaussian center

    const float exponent = d.x * d.x * frag_cov2inv.x +
                     d.y * d.y * frag_cov2inv.z +
                     2.0 * d.x * d.y * frag_cov2inv.y;

    const float alpha = frag_color.a * exp(-0.5 * exponent);

    if (alpha <= THRESHOLD)
        discard;


    float randomVal = randomUniform(u_randomSeed);

    if (randomVal < alpha)
        out_color = vec4(frag_color.rgb, 1.0);
    else
        discard;
}