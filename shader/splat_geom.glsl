/*
    Copyright (c) 2024 Anthony J. Thibault
    This software is licensed under the MIT License. See LICENSE for more details.

     Edited by Shakiba Kheradmand, 2025
*/

/*%%HEADER%%*/

uniform vec3 projParams;  // x, y, WIDTH, HEIGHT

layout(points) in;
layout(triangle_strip, max_vertices = 4) out;

in vec4 geom_color[];  // radiance of splat
in vec3 geom_cov2inv[];  // 2D screen space covariance matrix of the gaussian
in vec4 geom_uv[];
in vec2 geom_p[];  // the 2D screen space center of the gaussian

out vec4 frag_color;  // radiance of splat
out vec3 frag_cov2inv;  // inverse of the 2D screen space covariance matrix of the guassian
out vec2 frag_p;  // the 2D screen space center of the gaussian

void main()
{
    // discard splats that end up outside of a guard band
    vec4 pos = gl_in[0].gl_Position;
    if (pos.z < 0.2 || abs(pos.x) > pos.w || abs(pos.y) > pos.w)
        return;
    // Pass along primitive ID so the fragment shader can randomize wrt to it.
    gl_PrimitiveID = gl_PrimitiveIDIn;

    float WIDTH = projParams.x;
    float HEIGHT = projParams.y;

    float w = gl_in[0].gl_Position.w;
    vec2 scaleFactors = vec2((2.0f / WIDTH) * w, (2.0f / HEIGHT) * w);

    vec2 u1 = geom_uv[0].xy * scaleFactors;
    vec2 u2 = geom_uv[0].zw * scaleFactors;

    vec2 majAxis = geom_uv[0].xy;
    vec2 minAxis = geom_uv[0].zw;

    vec2 offsets[4];
    offsets[0] = majAxis + minAxis;
    offsets[1] = -majAxis + minAxis;
    offsets[3] = -majAxis - minAxis;
    offsets[2] = majAxis - minAxis;

    vec2 offset;
    for (int i = 0; i < 4; i++) {
      // transform offset back into clip space, and apply it to gl_Position.
      offset = offsets[i] * scaleFactors;

      gl_Position = gl_in[0].gl_Position + vec4(offset.xy, 0.0, 0.0);
      frag_color = geom_color[0];
      frag_cov2inv = geom_cov2inv[0];
      frag_p = geom_p[0];

      EmitVertex();
    }

    EndPrimitive();

    /*int N = 4;
    float pre_angle = 2.0 * 3.14159 / float(N);
   
    int i = 0;
    float angle = pre_angle * float(i % N);
    vec2 offset = cos(angle) * u1 + sin(angle) * u2;

    gl_Position = gl_in[0].gl_Position + vec4(offset.xy, 0.0, 0.0);
    frag_color = geom_color[0];
    frag_cov2inv = geom_cov2inv[0];
    frag_p = geom_p[0];

    EmitVertex();
    for (int i = 1; i < (N/2); i++)
    {
        angle = pre_angle * float(i % N);
        offset = cos(angle) * u1 + sin(angle) * u2;

        gl_Position = gl_in[0].gl_Position + vec4(offset.xy, 0.0, 0.0);
        frag_color = geom_color[0];
        frag_cov2inv = geom_cov2inv[0];
        frag_p = geom_p[0];

        EmitVertex();

        angle = pre_angle * float((N - i) % N);
        offset = cos(angle) * u1 + sin(angle) * u2;

        gl_Position = gl_in[0].gl_Position + vec4(offset.xy, 0.0, 0.0);
        frag_color = geom_color[0];
        frag_cov2inv = geom_cov2inv[0];
        frag_p = geom_p[0];

        EmitVertex();
    }
    i = (N/2);
    angle = pre_angle * float(i % N);
    offset = cos(angle) * u1 + sin(angle) * u2;

    gl_Position = gl_in[0].gl_Position + vec4(offset.xy, 0.0, 0.0);
    frag_color = geom_color[0];
    frag_cov2inv = geom_cov2inv[0];
    frag_p = geom_p[0];

    EmitVertex();

    EndPrimitive();*/
}