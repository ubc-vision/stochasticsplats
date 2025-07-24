/*
    Copyright (c) 2024 Anthony J. Thibault
    This software is licensed under the MIT License. See LICENSE for more details.

    Edited by Shakiba Kheradmand, 2025
*/

/*%%HEADER%%*/

in vec4 frag_color;  // radiance of splat
in vec3 frag_cov2inv;  // inverse of the 2D screen space covariance matrix of the guassian
in vec2 frag_p;  // 2D screen space center of the guassian

out vec4 out_color;

void main()
{
    vec2 d = gl_FragCoord.xy - frag_p;

    float g =
        exp(-0.5f * (d.x * d.x * frag_cov2inv.x + d.y * d.y * frag_cov2inv.z) -
            d.x * d.y * frag_cov2inv.y);

    out_color.rgb = frag_color.a * g * frag_color.rgb;
    out_color.a = frag_color.a * g;


    if ((frag_color.a * g) <= (1.0f / 255.0f))
    {
        discard;
    }
}